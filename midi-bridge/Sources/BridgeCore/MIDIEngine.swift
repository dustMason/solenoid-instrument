import CoreMIDI
import Foundation

public struct MIDIEndpoint: Identifiable, Equatable {
    public let ref: MIDIEndpointRef
    public let id: MIDIUniqueID
    public let name: String
    public let entity: MIDIEntityRef
}

public struct SavedEndpoint: Codable, Equatable {
    public let id: MIDIUniqueID
    public let name: String
    public init(id: MIDIUniqueID, name: String) { self.id = id; self.name = name }
    public init(_ endpoint: MIDIEndpoint) { self.init(id: endpoint.id, name: endpoint.name) }
}

public enum EndpointSelection {
    /// Never substitute an arbitrary device when the chosen device is unplugged.
    public static func resolve(_ saved: SavedEndpoint?, in endpoints: [MIDIEndpoint],
                               defaultName: (String) -> Bool) -> MIDIEndpoint? {
        if let saved {
            if let exact = endpoints.first(where: { $0.id == saved.id && $0.name == saved.name }) { return exact }
            let matches = endpoints.filter { $0.name == saved.name }
            return matches.count == 1 ? matches[0] : nil
        }
        let matches = endpoints.filter { defaultName($0.name.lowercased()) }
        return matches.count == 1 ? matches[0] : nil
    }
}

public struct MIDIError: Error, LocalizedError {
    public let operation: String
    public let status: OSStatus
    public var errorDescription: String? { "\(operation) failed (\(status))." }
}

func check(_ status: OSStatus, _ operation: String) throws {
    if status != noErr { throw MIDIError(operation: operation, status: status) }
}

public struct EngineActivity {
    public let packets: UInt64
    public let lastReceived: TimeInterval
    public let sendError: OSStatus
}

/// CoreMIDI calls the receiver on its own high-priority thread. Routing and panic
/// share one short lock so an old callback cannot fire a note after All Off.
public final class MIDIEngine {
    private var client: MIDIClientRef = 0
    private var input: MIDIPortRef = 0
    private var output: MIDIPortRef = 0
    private let lock = NSLock()
    private var source: MIDIEndpointRef = 0
    private var destination: MIDIEndpointRef = 0
    private var generation = 0
    private var active = false
    private var held = [Bool](repeating: false, count: 16 * 128)
    private var packets: UInt64 = 0
    private var lastReceived: TimeInterval = 0
    private var sendError: OSStatus = noErr
    public var onSetupChanged: (() -> Void)?

    public init() throws {
        do {
            try check(MIDIClientCreateWithBlock("MIDI Bridge" as CFString, &client) { [weak self] _ in
                DispatchQueue.main.async { [weak self] in self?.onSetupChanged?() }
            }, "Opening MIDI")
            try check(MIDIOutputPortCreate(client, "Forward" as CFString, &output), "Opening output")
            try check(MIDIInputPortCreateWithProtocol(client, "Receive" as CFString, ._1_0, &input) { [weak self] list, context in
                self?.receive(list, generation: Int(bitPattern: context))
            }, "Opening input")
        } catch {
            if client != 0 { MIDIClientDispose(client) }
            client = 0
            throw error
        }
    }

    deinit {
        disconnect()
        if client != 0 { MIDIClientDispose(client) }
    }

    public static func endpoints(sources: Bool) -> [MIDIEndpoint] {
        let count = sources ? MIDIGetNumberOfSources() : MIDIGetNumberOfDestinations()
        return (0..<count).compactMap { index in
            let ref = sources ? MIDIGetSource(index) : MIDIGetDestination(index)
            var offline: Int32 = 0
            MIDIObjectGetIntegerProperty(ref, kMIDIPropertyOffline, &offline)
            guard offline == 0 else { return nil }
            var id: Int32 = 0
            guard MIDIObjectGetIntegerProperty(ref, kMIDIPropertyUniqueID, &id) == noErr else { return nil }
            var name: Unmanaged<CFString>?
            MIDIObjectGetStringProperty(ref, kMIDIPropertyDisplayName, &name)
            if name == nil { MIDIObjectGetStringProperty(ref, kMIDIPropertyName, &name) }
            var entity: MIDIEntityRef = 0
            MIDIEndpointGetEntity(ref, &entity)
            return MIDIEndpoint(ref: ref, id: id, name: name?.takeRetainedValue() as String? ?? "MIDI device", entity: entity)
        }.sorted { $0.name.localizedStandardCompare($1.name) == .orderedAscending }
    }

    /// Called on the main thread. No CoreMIDI connect/disconnect call is made
    /// while holding the callback lock.
    public func connect(source newSource: MIDIEndpointRef, destination newDestination: MIDIEndpointRef) throws {
        disconnect()
        lock.lock()
        generation += 1
        let token = generation
        source = newSource
        destination = newDestination
        sendError = noErr
        // Clear stale state on a reattached instrument before accepting notes.
        panicLocked()
        active = true
        lock.unlock()
        let status = MIDIPortConnectSource(input, newSource, UnsafeMutableRawPointer(bitPattern: token))
        if status != noErr {
            disconnect()
            throw MIDIError(operation: "Connecting input", status: status)
        }
    }

    public func disconnect() {
        lock.lock()
        let oldSource = source
        active = false
        generation += 1
        if destination != 0 { panicLocked() }
        source = 0
        destination = 0
        lock.unlock()
        if oldSource != 0 && input != 0 { MIDIPortDisconnectSource(input, oldSource) }
    }

    public func activity() -> EngineActivity {
        lock.lock(); defer { lock.unlock() }
        return EngineActivity(packets: packets, lastReceived: lastReceived, sendError: sendError)
    }

    private func receive(_ list: UnsafePointer<MIDIEventList>, generation token: Int) {
        lock.lock(); defer { lock.unlock() }
        guard active, token == generation, destination != 0 else { return }
        // Preserve all event data and timestamps, including CC, clock, transport,
        // pressure, pitch bend and SysEx. No parsing/reconstruction in the path.
        let status = MIDISendEventList(output, destination, list)
        guard status == noErr else {
            sendError = status
            active = false
            panicLocked()
            return
        }
        packets += UInt64(list.pointee.numPackets)
        lastReceived = Date.timeIntervalSinceReferenceDate
        do {
            var packet = UnsafeRawPointer(list).advanced(by: MemoryLayout<MIDIEventList>.offset(of: \.packet)!).assumingMemoryBound(to: MIDIEventPacket.self)
            for _ in 0..<list.pointee.numPackets {
                do {
                    let words = UnsafeRawPointer(packet).advanced(by: MemoryLayout<MIDIEventPacket>.offset(of: \.words)!).assumingMemoryBound(to: UInt32.self)
                    var i = 0
                    while i < Int(packet.pointee.wordCount) {
                        let word = words[i]
                        let type = Int(word >> 28)
                        if type == 2 {
                            let status = Int((word >> 16) & 0xff)
                            let note = Int((word >> 8) & 0x7f)
                            let channel = status & 15
                            let kind = status & 0xf0
                            if kind == 0x90 { held[channel * 128 + note] = (word & 0x7f) != 0 }
                            if kind == 0x80 { held[channel * 128 + note] = false }
                            if kind == 0xb0 && (note == 120 || note == 123) {
                                for n in 0..<128 { held[channel * 128 + n] = false }
                            }
                        }
                        // UMP widths (including SysEx payload words that may
                        // otherwise resemble a Note On status).
                        let widths = [1, 1, 1, 2, 2, 4, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4]
                        i += widths[type]
                    }
                }
                packet = UnsafePointer(MIDIEventPacketNext(packet))
            }
        }
    }

    private func panicLocked() {
        guard destination != 0, output != 0 else { return }
        MIDIFlushOutput(destination)
        var words: [UInt32] = []
        for channel in 0..<16 {
            for note in 0..<128 where held[channel * 128 + note] {
                words.append(0x20000000 | UInt32(0x80 | channel) << 16 | UInt32(note) << 8)
            }
            for cc in [64, 123, 120] {
                words.append(0x20000000 | UInt32(0xb0 | channel) << 16 | UInt32(cc) << 8)
            }
        }
        withMIDIEvents(words: words, timestamp: 0) { _ = MIDISendEventList(output, destination, $0) }
        held = [Bool](repeating: false, count: 16 * 128)
    }
}

/// Allocate enough room for variable-length CoreMIDI lists; never copy packets
/// into the fixed-size Swift tuple when traversing a list.
public func withMIDIEvents<T>(words: [UInt32], timestamp: MIDITimeStamp,
                              _ body: (UnsafePointer<MIDIEventList>) throws -> T) rethrows -> T {
    precondition(!words.isEmpty && words.count < 16000)
    let size = max(MemoryLayout<MIDIEventList>.size, 64 + words.count * 4)
    let raw = UnsafeMutableRawPointer.allocate(byteCount: size, alignment: 8)
    defer { raw.deallocate() }
    let list = raw.bindMemory(to: MIDIEventList.self, capacity: 1)
    let first = MIDIEventListInit(list, ._1_0)
    words.withUnsafeBufferPointer { buffer in
        _ = MIDIEventListAdd(list, size, first, timestamp, buffer.count, buffer.baseAddress!)
    }
    return try body(UnsafePointer(list))
}
