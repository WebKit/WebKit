// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "crash-test",
    targets: [
        .executableTarget(
            name: "crash-test",
            path: "Sources/crash-test",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "--export=crash_on_zero"])
            ]
        )
    ]
)
