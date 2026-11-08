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
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKWebsiteDataStoreConfiguration
private import TestWebKitAPILibrary.InjectedBundle.cocoa.WebProcessPlugIn.WebProcessPlugInWithInternals

extension WebPage.Configuration {
    /// Creates a new `WebPage.Configuration` initialized using a custom web process test plug-in class.
    ///
    /// - Parameters:
    ///   - testPlugInClass: The type of the plug-in class to use.
    ///   - configureJSCForTesting: If `true`, relaxes JSC's security hardening so that tests can freely modify JSC options, config,
    ///   and behavior that would otherwise be more secured.
    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Intentionally uses a custom process pool")
    #endif
    public init(testPlugInClass: (some WKWebProcessPlugIn).Type, configureJSCForTesting: Bool = true) {
        self.init()

        let processPoolConfiguration = _WKProcessPoolConfiguration()
        processPoolConfiguration.injectedBundleURL = Bundle.testPlugInURL
        processPoolConfiguration.configureJSCForTesting = configureJSCForTesting

        // This is never actually nil; `WKProcessPoolPrivate.h` does not have proper nullability annotations.
        // swift-format-ignore: NeverForceUnwrap
        let processPool = WKProcessPool._processPool(with: processPoolConfiguration)!
        processPool._setObject(NSStringFromClass(testPlugInClass) as NSString, forBundleParameter: "TestPlugInPrincipalClassName")

        self.processPool = processPool
    }

    /// Creates a new `WebPage.Configuration` whose web process installs the `internals` object on every frame.
    ///
    /// - Parameter:
    ///   - configureJSCForTesting: If `true`, relaxes JSC's security hardening so that tests can freely modify JSC options, config,
    ///   and behavior that would otherwise be more secured.
    /// - Returns: A correctly-configured WebPage.Configuration.
    public static func withInternals(configureJSCForTesting: Bool = true) -> WebPage.Configuration {
        .init(testPlugInClass: WebProcessPlugInWithInternals.self, configureJSCForTesting: configureJSCForTesting)
    }
}

extension WebPage.Configuration {
    /// Creates a configuration whose website data store sends HTTPS loads through a server's proxy.
    ///
    /// See ``WebKit/WKWebViewConfiguration/init(httpsProxyFor:)`` for how the server handles those loads. Use the
    /// configuration with a navigation decider that trusts the server's certificate, such as
    /// ``NavigationDeciderAllowingAnyTLSCertificate``.
    ///
    /// - Parameter server: The configuration of the server.
    public init(httpsProxyFor server: HTTPServer.Configuration) {
        self.init()

        let storeConfiguration = _WKWebsiteDataStoreConfiguration(nonPersistentConfiguration: ())
        storeConfiguration.httpsProxy = server.httpsProxy
        websiteDataStore = WKWebsiteDataStore._store(with: storeConfiguration)
    }
}

#endif // ENABLE_SWIFTUI
