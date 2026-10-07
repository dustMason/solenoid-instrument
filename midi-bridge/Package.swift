// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "MIDIBridge",
    platforms: [.macOS(.v12)],
    products: [.executable(name: "MIDIBridge", targets: ["MIDIBridge"])],
    targets: [
        .target(name: "BridgeCore"),
        .executableTarget(name: "MIDIBridge", dependencies: ["BridgeCore"]),
        .testTarget(name: "BridgeCoreTests", dependencies: ["BridgeCore"])
    ]
)
