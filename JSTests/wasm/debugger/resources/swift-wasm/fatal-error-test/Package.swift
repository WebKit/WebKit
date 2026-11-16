// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "fatal-error-test",
    targets: [
        .executableTarget(
            name: "fatal-error-test",
            path: "Sources/fatal-error-test",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "--export=trigger_fatal_error"])
            ]
        )
    ]
)
