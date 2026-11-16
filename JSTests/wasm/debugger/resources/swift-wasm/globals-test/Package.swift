// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "globals-test",
    targets: [
        .executableTarget(
            name: "globals-test",
            path: "Sources/globals-test",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "--export=increment_globals"])
            ]
        )
    ]
)
