// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import Foundation
import struct Swift.String

#if WTF_PLATFORM_MAC
private import AppKit
#endif

@main
struct TestWebKitAPI {
    private static func forceSiteIsolationForTesting() {
        let theClass = NSClassFromString("WKPreferences") as? NSObject.Type
        let sel = Selector(("_forceSiteIsolationAlwaysOnForTesting"))
        if let theClass, theClass.responds(to: sel) {
            unsafe theClass.perform(sel)
        }
    }

    private func handleArguments(_ argumentDefaults: inout [String: Any]) {
        // FIXME: We should switch these defaults to use overlay scrollbars, since they are the
        // default on the platform, but a variety of tests will need changes.
        argumentDefaults["NSOverlayScrollersEnabled"] = false
        argumentDefaults["AppleShowScrollBars"] = "Always"

        // FIXME: Remove once the root cause is fixed in rdar://159372811
        argumentDefaults["NSEventConcurrentProcessingEnabled"] = false

        for argument in CommandLine.arguments[1...] {
            // These defaults are not propagated manually but are only consulted in the UI process.
            switch argument {
            case "--remote-layer-tree":
                argumentDefaults["WebKit2UseRemoteLayerTreeDrawingArea"] = true
            case "--no-remote-layer-tree":
                argumentDefaults["WebKit2UseRemoteLayerTreeDrawingArea"] = false
            case "--use-gpu-process":
                argumentDefaults["WebKit2GPUProcessForDOMRendering"] = true
            case "--no-use-gpu-process":
                argumentDefaults["WebKit2GPUProcessForDOMRendering"] = false
            case "--site-isolation-enabled-by-default":
                Self.forceSiteIsolationForTesting()
            default:
                break
            }
        }
    }

    @MainActor
    private func configure() -> TestRunner.Configuration {
        #if WTF_PLATFORM_MACCATALYST
        UINSApplicationInstantiate()
        #endif

        UserDefaults.standard.removePersistentDomain(forName: "TestWebKitAPI")

        var argumentDomain = UserDefaults.standard.volatileDomain(forName: UserDefaults.argumentDomain)

        #if WTF_PLATFORM_MAC
        // CAUTION: Defaults set here are not automatically propagated to the
        // Web Content process. Those listed below are propagated manually.
        handleArguments(&argumentDomain)
        #endif

        UserDefaults.standard.setVolatileDomain(argumentDomain, forName: UserDefaults.argumentDomain)

        #if !WTF_PLATFORM_MAC
        let uiKitDefaults = UserDefaults(suiteName: "com.apple.UIKit")
        uiKitDefaults?
            .register(
                defaults: [
                    "ForceLegacyHostingRemoteViewControllerForService": "com.apple.ScreenTime.ScreenTimeWebExtension"
                ]
            )
        #endif // !WTF_PLATFORM_MAC

        TestWebKitAPIEnableAllSDKAlignedBehaviors()

        #if WTF_PLATFORM_MAC
        _ = NSApplication.shared
        #endif

        return TestRunner.Configuration(parsing: CommandLine.arguments)
    }

    @MainActor
    private func runGoogleTests(with configuration: TestRunner.Configuration) async throws -> (passed: Bool, didRun: Bool) {
        let (passed, didRun) = try await GoogleTestsController.shared.run(with: configuration)
        if didRun && !configuration.listTests {
            exit(passed ? EXIT_SUCCESS : EXIT_FAILURE)
        }

        return (passed, didRun)
    }

    @MainActor
    private func runSwiftTests(with configuration: TestRunner.Configuration, googleTestsPassed: Bool) async throws {
        let (passed, didRun) = try await SwiftTestsController.shared.run(with: configuration)
        if didRun && !configuration.listTests {
            exit(passed ? EXIT_SUCCESS : EXIT_FAILURE)
        }

        exit(googleTestsPassed && passed ? EXIT_SUCCESS : EXIT_FAILURE)
    }

    static func main() async throws {
        let instance = TestWebKitAPI()

        let configuration = instance.configure()

        if configuration.help {
            print(configuration.usage)
            exit(EXIT_SUCCESS)
        }

        let (googleTestsPassed, didRunGoogleTests) = try await instance.runGoogleTests(with: configuration)

        #if WTF_PLATFORM_MAC
        // Some tests synthesize HID events that the system delivers only to the frontmost app, and
        // delivery requires a running AppKit event loop. Only start it when we will actually run
        // Swift tests: i.e. when running (not listing) tests and no GoogleTest tests have run.
        if !configuration.listTests && !didRunGoogleTests {
            let application = NSApplication.shared
            application.setActivationPolicy(.regular)

            Task { @MainActor in
                do {
                    try await instance.runSwiftTests(with: configuration, googleTestsPassed: googleTestsPassed)
                } catch {
                    exit(EXIT_FAILURE)
                }
            }

            application.activate(ignoringOtherApps: true)
            application.run()
            return
        }
        #endif

        // Non-macOS, or macOS when listing tests / after GoogleTest tests ran: no event loop needed.
        try await instance.runSwiftTests(with: configuration, googleTestsPassed: googleTestsPassed)
    }
}
