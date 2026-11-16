// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "func-b",
    targets: [
        .executableTarget(
            name: "func-b",
            path: "Sources/func-b",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "--export=func_b"])
            ]
        )
    ]
)
