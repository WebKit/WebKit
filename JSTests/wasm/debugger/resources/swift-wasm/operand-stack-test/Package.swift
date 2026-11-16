// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "operand-stack-test",
    targets: [
        .executableTarget(
            name: "operand-stack-test",
            path: "Sources/operand-stack-test",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "--export=entry"])
            ]
        )
    ]
)
