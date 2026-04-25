// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "func-a",
    targets: [
        .executableTarget(
            name: "func-a",
            path: "Sources/func-a",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "--export=func_a"])
            ]
        )
    ]
)
