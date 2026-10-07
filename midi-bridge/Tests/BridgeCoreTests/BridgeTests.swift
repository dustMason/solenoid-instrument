import XCTest
import CoreMIDI
import Darwin
@testable import BridgeCore

private final class Capture {
    private let lock = NSLock()
    private var values: [UInt32] = []
    private var times: [MIDITimeStamp] = []
    func receive(_ list: UnsafePointer<MIDIEventList>) {
        lock.lock(); defer { lock.unlock() }
        var packet = UnsafeRawPointer(list).advanced(by: MemoryLayout<MIDIEventList>.offset(of: \.packet)!).assumingMemoryBound(to: MIDIEventPacket.self)
        for _ in 0..<list.pointee.numPackets {
            let words = UnsafeRawPointer(packet).advanced(by: MemoryLayout<MIDIEventPacket>.offset(of: \.words)!).assumingMemoryBound(to: UInt32.self)
            values += Array(UnsafeBufferPointer(start: words, count: Int(packet.pointee.wordCount)))
            times.append(packet.pointee.timeStamp)
            packet = UnsafePointer(MIDIEventPacketNext(packet))
        }
    }
    var words: [UInt32] { lock.lock(); defer { lock.unlock() }; return values }
    var timestamps: [MIDITimeStamp] { lock.lock(); defer { lock.unlock() }; return times }
    func clear() { lock.lock(); values = []; times = []; lock.unlock() }
}

private final class MIDIFixture {
    var client: MIDIClientRef = 0
    var source: MIDIEndpointRef = 0
    var destination: MIDIEndpointRef = 0
    let capture = Capture()
    let suffix = UUID().uuidString
    var sourceName: String { "Bridge Test Source \(suffix)" }
    var destinationName: String { "Bridge Test Destination \(suffix)" }
    init() throws {
        try check(MIDIClientCreateWithBlock("Bridge Test" as CFString, &client, nil), "Test client")
        try createSource()
        try createDestination()
    }
    func createSource() throws {
        try check(MIDISourceCreateWithProtocol(client, sourceName as CFString, ._1_0, &source), "Test source")
    }
    func createDestination() throws {
        try check(MIDIDestinationCreateWithProtocol(client, destinationName as CFString, ._1_0, &destination) { [capture] list, _ in
            capture.receive(list)
        }, "Test destination")
    }
    func removeSource() { MIDIEndpointDispose(source); source = 0 }
    func removeDestination() { MIDIEndpointDispose(destination); destination = 0 }
    func send(_ words: [UInt32], timestamp: MIDITimeStamp = mach_absolute_time()) throws {
        try withMIDIEvents(words: words, timestamp: timestamp) { try check(MIDIReceivedEventList(source, $0), "Test send") }
    }
    deinit { MIDIClientDispose(client) }
    var sourceEndpoint: MIDIEndpoint { MIDIEngine.endpoints(sources: true).first { $0.ref == source }! }
    var destEndpoint: MIDIEndpoint { MIDIEngine.endpoints(sources: false).first { $0.ref == destination }! }
}

final class BridgeTests: XCTestCase {
    func pump(_ duration: TimeInterval = 0.08) {
        RunLoop.current.run(until: Date().addingTimeInterval(duration))
    }
    func wait(_ predicate: () -> Bool, file: StaticString = #filePath, line: UInt = #line) {
        let end = Date().addingTimeInterval(3)
        while !predicate() && Date() < end { pump(0.01) }
        XCTAssertTrue(predicate(), file: file, line: line)
    }

    func testForwardingPreservesMessagesChannelsAndTimestamps() throws {
        let fixture = try MIDIFixture()
        let engine = try MIDIEngine()
        try engine.connect(source: fixture.source, destination: fixture.destination)
        pump(); fixture.capture.clear()
        // Notes on channels 1, 2 and 10; zero-velocity note-off; CC; pressure;
        // program; pitch bend; clock/start/continue/stop; a complete SysEx7.
        let words: [UInt32] = [0x20903c64, 0x20913e7f, 0x20992450, 0x20903c00,
                              0x20813e00, 0x20b11464, 0x20a13e30, 0x20d12000,
                              0x20c10500, 0x20e10040, 0x10f80000, 0x10fa0000,
                              0x10fb0000, 0x10fc0000, 0x30037d01, 0x02000000]
        let timestamp = mach_absolute_time()
        try fixture.send(words, timestamp: timestamp)
        wait { fixture.capture.words.count >= words.count }
        XCTAssertEqual(fixture.capture.words, words)
        XCTAssertTrue(fixture.capture.timestamps.allSatisfy { $0 == timestamp })
        XCTAssertEqual(engine.activity().sendError, noErr)
    }

    func testLargeVariableLengthPacketsAndMultiplePackets() throws {
        let fixture = try MIDIFixture()
        let engine = try MIDIEngine()
        try engine.connect(source: fixture.source, destination: fixture.destination)
        pump(); fixture.capture.clear()
        let words = (0..<300).map { UInt32(0x20b01400) | UInt32($0 % 128) }
        try fixture.send(words)
        try fixture.send([0x20913f7f, 0x20813f00])
        wait { fixture.capture.words.count >= 302 }
        XCTAssertEqual(fixture.capture.words, words + [0x20913f7f, 0x20813f00])
        XCTAssertGreaterThanOrEqual(engine.activity().packets, 2)
    }

    func testDisconnectReleasesHeldNotesAndBlocksNewNotes() throws {
        let fixture = try MIDIFixture()
        let engine = try MIDIEngine()
        try engine.connect(source: fixture.source, destination: fixture.destination)
        pump(); fixture.capture.clear()
        try fixture.send([0x20903c7f, 0x20913e64, 0x20992450])
        wait { fixture.capture.words.count == 3 }
        fixture.capture.clear()
        engine.disconnect()
        wait { fixture.capture.words.count >= 51 }
        XCTAssertTrue(fixture.capture.words.contains(0x20803c00))
        XCTAssertTrue(fixture.capture.words.contains(0x20813e00))
        XCTAssertTrue(fixture.capture.words.contains(0x20892400))
        XCTAssertTrue(fixture.capture.words.contains(0x20b97800))
        let count = fixture.capture.words.count
        try fixture.send([0x2099407f]); pump()
        XCTAssertEqual(fixture.capture.words.count, count)
    }

    func testSessionReconnectsBothDevicesAndRemembersPause() throws {
        let fixture = try MIDIFixture()
        let domain = "com.dustmason.MIDIBridge.Tests.\(UUID().uuidString)"
        let defaults = UserDefaults(suiteName: domain)!
        defer { defaults.removePersistentDomain(forName: domain) }
        // Explicit identities ensure this test never selects real hardware.
        defaults.set(try JSONEncoder().encode(SavedEndpoint(fixture.sourceEndpoint)), forKey: "source")
        defaults.set(try JSONEncoder().encode(SavedEndpoint(fixture.destEndpoint)), forKey: "destination")
        let session = BridgeSession(defaults: defaults, poll: false)
        XCTAssertTrue(session.connected)
        fixture.removeSource(); session.refresh()
        XCTAssertFalse(session.connected)
        try fixture.createSource(); session.refresh()
        XCTAssertTrue(session.connected)
        fixture.removeDestination(); session.refresh()
        XCTAssertFalse(session.connected)
        try fixture.createDestination(); session.refresh()
        XCTAssertTrue(session.connected)
        pump(); fixture.capture.clear()
        try fixture.send([0x20903c7f])
        wait { fixture.capture.words.contains(0x20903c7f) }
        session.allOff(); pump()
        XCTAssertFalse(session.connected)
        XCTAssertFalse(session.enabled)
        XCTAssertTrue(fixture.capture.words.contains(0x20803c00))
        session.shutdown()
        let restarted = BridgeSession(defaults: defaults, poll: false)
        XCTAssertFalse(restarted.enabled)
        XCTAssertFalse(restarted.connected)
        restarted.setEnabled(true)
        XCTAssertTrue(restarted.connected)
        restarted.shutdown()
    }

    func testSelectionDoesNotGuessWhenMissingOrAmbiguous() {
        let a = MIDIEndpoint(ref: 1, id: 10, name: "Teensy MIDI", entity: 0)
        let b = MIDIEndpoint(ref: 2, id: 20, name: "Teensy MIDI", entity: 0)
        XCTAssertNil(EndpointSelection.resolve(nil, in: [a, b], defaultName: { $0.contains("teensy") }))
        XCTAssertNil(EndpointSelection.resolve(SavedEndpoint(id: 99, name: "Missing"), in: [a], defaultName: { _ in true }))
        XCTAssertEqual(EndpointSelection.resolve(SavedEndpoint(a), in: [a, b], defaultName: { _ in true }), a)
        XCTAssertEqual(EndpointSelection.resolve(SavedEndpoint(id: 99, name: "Teensy MIDI"), in: [a], defaultName: { _ in true }), a)
    }
}
