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

#if ENABLE_SWIFTUI

@_spi(Testing) public import WebKit
public import WebKit_Private.WKWebProcessPlugIn
private import WebKit_Private.WKProcessPoolPrivate

extension WebPage.Configuration {
    /// Creates a new `WebPage.Configuration` initialized using a custom web process test plug-in class.
    ///
    /// For example, to create a configuration to allow a WebPage to access the `Internals` plug-in:
    ///
    /// ```
    ///  let configuration = WebPage.Configuration(testPlugInClass: WebProcessPlugInWithInternals.self)
    /// ```
    ///
    /// - Parameters:
    ///   - testPlugInClass: The type of the plug-in class to use.
    ///   - configureJSCForTesting: If `true`, relaxes JSC's security hardening so that tests can freely modify JSC options, config,
    ///   and behavior that would otherwise be more secured.
    public init(testPlugInClass: (some WKWebProcessPlugIn).Type, configureJSCForTesting: Bool = true) {
        guard let bundleURL = Bundle.main.url(forResource: "TestWebKitAPI", withExtension: "wkbundle") else {
            preconditionFailure("No TestWebKitAPI bundle url found.")
        }

        self.init()

        let processPoolConfiguration = _WKProcessPoolConfiguration()
        processPoolConfiguration.injectedBundleURL = bundleURL
        processPoolConfiguration.configureJSCForTesting = configureJSCForTesting

        // This is never actually nil; `WKProcessPoolPrivate.h` does not have proper nullability annotations.
        // swift-format-ignore: NeverForceUnwrap
        let processPool = WKProcessPool._processPool(with: processPoolConfiguration)!
        processPool._setObject(NSStringFromClass(testPlugInClass) as NSString, forBundleParameter: "TestPlugInPrincipalClassName")

        self.processPool = processPool
    }
}

#endif // ENABLE_SWIFTUI
