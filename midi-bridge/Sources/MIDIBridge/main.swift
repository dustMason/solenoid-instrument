import AppKit
import SwiftUI
import BridgeCore

struct BridgeView: View {
    @ObservedObject var session: BridgeSession

    var body: some View {
        VStack(alignment: .leading, spacing: 22) {
            HStack(spacing: 12) {
                Image(systemName: "point.3.connected.trianglepath.dotted")
                    .font(.system(size: 28)).foregroundColor(.accentColor)
                VStack(alignment: .leading, spacing: 3) {
                    Text("MIDI Bridge").font(.title2.weight(.semibold))
                    Text("BeatStep Pro → solenoids").foregroundColor(.secondary)
                }
                Spacer()
            }
            VStack(alignment: .leading, spacing: 14) {
                endpointPicker("From", endpoints: session.sources, saved: session.savedSource,
                               automatic: "Automatic · BeatStep Pro", select: session.selectSource)
                endpointPicker("To", endpoints: session.destinations, saved: session.savedDestination,
                               automatic: "Automatic · Teensy / Solenoid", select: session.selectDestination)
            }
            HStack(alignment: .top, spacing: 9) {
                Circle().fill(session.connected ? Color.green : Color.orange).frame(width: 8, height: 8).padding(.top, 5)
                Text(session.status).font(.callout).fixedSize(horizontal: false, vertical: true)
                Spacer(minLength: 0)
            }
            HStack {
                HStack(spacing: 6) {
                    Circle().fill(session.recentActivity ? Color.green : Color.secondary.opacity(0.25)).frame(width: 6, height: 6)
                    Text(session.packetCount == 0 ? "No MIDI received yet" : "MIDI received · \(session.packetCount.formatted())")
                        .font(.caption).foregroundColor(.secondary).monospacedDigit()
                }
                Spacer()
                Button(session.enabled ? "Pause" : "Resume") { session.setEnabled(!session.enabled) }
                Button("All Off") { session.allOff() }.disabled(!session.connected)
                    .help("Stop forwarding and release notes. Click Resume to play again.")
            }
            Divider()
            Text("Plug both devices into this Mac. Your choices are remembered and reconnect automatically. Keep the lid open while playing.")
                .font(.caption).foregroundColor(.secondary).fixedSize(horizontal: false, vertical: true)
        }
        .padding(24)
        .frame(width: 460)
    }

    private func endpointPicker(_ label: String, endpoints: [MIDIEndpoint], saved: SavedEndpoint?,
                                automatic: String, select: @escaping (Int32) -> Void) -> some View {
        HStack {
            Text(label).foregroundColor(.secondary).frame(width: 40, alignment: .leading)
            Picker(label, selection: Binding(get: { saved?.id ?? 0 }, set: select)) {
                Text(automatic).tag(Int32(0))
                if let saved, !endpoints.contains(where: { $0.id == saved.id }) {
                    Text("\(saved.name) (disconnected)").tag(saved.id)
                }
                ForEach(endpoints) { endpoint in Text(endpoint.name).tag(endpoint.id) }
            }.labelsHidden().frame(maxWidth: .infinity)
        }
    }
}

final class AppDelegate: NSObject, NSApplicationDelegate {
    private var window: NSWindow?
    private var session: BridgeSession?
    private var awake: NSObjectProtocol?

    func applicationDidFinishLaunching(_ notification: Notification) {
        let session = BridgeSession()
        self.session = session
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 460, height: 350),
                              styleMask: [.titled, .closable, .miniaturizable], backing: .buffered, defer: false)
        window.title = "MIDI Bridge"
        window.contentView = NSHostingView(rootView: BridgeView(session: session))
        window.center()
        window.setFrameAutosaveName("MIDIBridgeWindow")
        window.isReleasedWhenClosed = false
        window.makeKeyAndOrderFront(nil)
        self.window = window
        awake = ProcessInfo.processInfo.beginActivity(options: [.userInitiated, .idleSystemSleepDisabled],
                                                     reason: "Forwarding MIDI for live instruments")
        NSWorkspace.shared.notificationCenter.addObserver(self, selector: #selector(willSleep),
                                                          name: NSWorkspace.willSleepNotification, object: nil)
        NSWorkspace.shared.notificationCenter.addObserver(self, selector: #selector(didWake),
                                                          name: NSWorkspace.didWakeNotification, object: nil)
        NSApp.activate(ignoringOtherApps: true)
    }

    @objc private func willSleep() { session?.allOff() }
    @objc private func didWake() { session?.refresh() }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationWillTerminate(_ notification: Notification) {
        session?.shutdown()
        if let awake { ProcessInfo.processInfo.endActivity(awake) }
    }
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
let menu = NSMenu()
let appItem = NSMenuItem()
let appMenu = NSMenu()
appMenu.addItem(withTitle: "Quit MIDI Bridge", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
appItem.submenu = appMenu
menu.addItem(appItem)
app.mainMenu = menu
app.run()
