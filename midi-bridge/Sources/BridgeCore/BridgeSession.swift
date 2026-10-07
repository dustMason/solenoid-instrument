import Combine
import CoreMIDI
import Foundation

public final class BridgeSession: ObservableObject {
    @Published public private(set) var sources: [MIDIEndpoint] = []
    @Published public private(set) var destinations: [MIDIEndpoint] = []
    @Published public private(set) var savedSource: SavedEndpoint?
    @Published public private(set) var savedDestination: SavedEndpoint?
    @Published public private(set) var enabled: Bool
    @Published public private(set) var connected = false
    @Published public private(set) var status = "Looking for MIDI devices…"
    @Published public private(set) var packetCount: UInt64 = 0
    @Published public private(set) var recentActivity = false
    private var engine: MIDIEngine?
    private var route: (MIDIEndpointRef, MIDIEndpointRef)?
    private let defaults: UserDefaults
    private var timer: Timer?
    private var tickCount = 0
    private var failure: String?

    public init(defaults: UserDefaults = .standard, poll: Bool = true) {
        self.defaults = defaults
        savedSource = Self.load("source", from: defaults)
        savedDestination = Self.load("destination", from: defaults)
        enabled = defaults.object(forKey: "enabled") as? Bool ?? true
        refresh()
        if poll {
            timer = Timer.scheduledTimer(withTimeInterval: 0.2, repeats: true) { [weak self] _ in self?.tick() }
            RunLoop.main.add(timer!, forMode: .common)
        }
    }

    deinit { timer?.invalidate(); engine?.disconnect() }

    public func selectSource(_ id: Int32) {
        savedSource = sources.first(where: { $0.id == id }).map(SavedEndpoint.init)
        persist(); failure = nil; refresh()
    }

    public func selectDestination(_ id: Int32) {
        savedDestination = destinations.first(where: { $0.id == id }).map(SavedEndpoint.init)
        persist(); failure = nil; refresh()
    }

    public func setEnabled(_ value: Bool) {
        enabled = value; failure = nil
        persist(); refresh()
    }

    /// Stops forwarding before releasing notes. Resuming requires an explicit click.
    public func allOff() { setEnabled(false) }

    public func shutdown() {
        timer?.invalidate(); timer = nil
        engine?.disconnect(); route = nil; connected = false
    }

    public func refresh() {
        if engine == nil {
            do {
                engine = try MIDIEngine()
                engine?.onSetupChanged = { [weak self] in self?.refresh() }
            } catch { status = error.localizedDescription; return }
        }
        sources = MIDIEngine.endpoints(sources: true)
        destinations = MIDIEngine.endpoints(sources: false)
        let source = EndpointSelection.resolve(savedSource, in: sources) { $0.contains("beatstep pro") }
        let dest = EndpointSelection.resolve(savedDestination, in: destinations) {
            $0.contains("teensy") || $0.contains("solenoid")
        }
        if let source { savedSource = SavedEndpoint(source) }
        if let dest { savedDestination = SavedEndpoint(dest) }
        persist()
        guard enabled else {
            stopRoute()
            status = failure ?? "Paused — all notes off"
            return
        }
        guard let source, let dest else {
            stopRoute()
            if source == nil && dest == nil { status = "Connect your BeatStep Pro and solenoid controller" }
            else if source == nil { status = "Waiting for \(savedSource?.name ?? "BeatStep Pro")" }
            else { status = "Waiting for \(savedDestination?.name ?? "solenoid controller")" }
            return
        }
        guard source.entity == 0 || source.entity != dest.entity else {
            stopRoute(); status = "Choose two different devices to avoid a MIDI loop"; return
        }
        if route?.0 != source.ref || route?.1 != dest.ref {
            do {
                try engine?.connect(source: source.ref, destination: dest.ref)
                route = (source.ref, dest.ref)
            } catch { stopRoute(); status = error.localizedDescription; return }
        }
        connected = true
        status = "Connected — ready to play"
    }

    public func tick() {
        if let activity = engine?.activity() {
            packetCount = activity.packets
            recentActivity = connected && Date.timeIntervalSinceReferenceDate - activity.lastReceived < 0.25
            if activity.sendError != noErr && enabled {
                failure = "MIDI output error (\(activity.sendError)). Reconnect and resume."
                enabled = false; persist(); refresh()
            }
        }
        tickCount += 1
        if tickCount % 10 == 0 { refresh() }
    }

    private func stopRoute() {
        if route != nil { engine?.disconnect() }
        route = nil; connected = false; recentActivity = false
    }

    private func persist() {
        defaults.set(savedSource.flatMap { try? JSONEncoder().encode($0) }, forKey: "source")
        defaults.set(savedDestination.flatMap { try? JSONEncoder().encode($0) }, forKey: "destination")
        defaults.set(enabled, forKey: "enabled")
    }

    private static func load(_ key: String, from defaults: UserDefaults) -> SavedEndpoint? {
        defaults.data(forKey: key).flatMap { try? JSONDecoder().decode(SavedEndpoint.self, from: $0) }
    }
}
