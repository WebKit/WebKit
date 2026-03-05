// swift-tools-version: 6.2
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "do-catch-throw",
    targets: [
        .executableTarget(
            name: "do-catch-throw",
            path: "Sources/test",
            linkerSettings: [
                .unsafeFlags(["-Xlinker", "--export=test_do_throw_catch"]),
                .unsafeFlags(["-Xlinker", "--export=test_do_throw_catch_nested"]),
                .unsafeFlags(["-Xlinker", "--export=test_do_throw_func_catch"]),
                .unsafeFlags(["-Xlinker", "--export=test_do_throw_func_catch_nested"])
            ]
        )
    ]
)
