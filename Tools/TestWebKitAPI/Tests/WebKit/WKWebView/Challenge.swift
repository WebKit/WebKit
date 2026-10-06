// Copyright (C) 2019-2026 Apple Inc. All rights reserved.
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
private import TestWebKitAPILibrary
private import TestWebKitAPILibrary.Helpers.cocoa.HTTPServer.TestCertificates
private import TestWebKitAPILibrary.Helpers.cocoa.TestNavigationDelegate
private import TestWebKitAPILibrary.Helpers.cocoa.WKWebViewConfigurationExtras
import Testing
import WebKit
private import WebKit_Private
private import WebKit_Private.WKProcessPoolPrivate
private import WebKit_Private.WKWebsiteDataStorePrivate
private import WebKit_Private._WKErrorRecoveryAttempting
private import WebKit_Private._WKWebsiteDataStoreConfiguration

import struct Foundation.URL
import struct Swift.String

@MainActor
struct ChallengeTests {
    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Clearing permanent credentials is only exposed on WKProcessPool")
    #endif
    @Test
    func secIdentity() async throws {
        var server = try HTTPServer(protocol: .http, connectionHandler: HTTPServer.respondWithChallengeThenOK)

        let webView = WKWebView()
        let delegate = TestNavigationDelegate()
        var challenges: [URLAuthenticationChallenge] = []
        delegate.didReceiveAuthenticationChallenge = { _, challenge, completionHandler in
            challenges.append(challenge)
            completionHandler(
                .useCredential,
                URLCredential(identity: TestCertificates.identity, certificates: [TestCertificates.certificate], persistence: .none)
            )
        }
        webView.navigationDelegate = delegate

        try await server.run { configuration in
            // Make sure no credential left by previous tests.
            let protectionSpace = URLProtectionSpace(
                host: "127.0.0.1",
                port: configuration.port,
                protocol: NSURLProtectionSpaceHTTP,
                realm: "testrealm",
                authenticationMethod: NSURLAuthenticationMethodHTTPBasic
            )
            webView.configuration.processPool._clearPermanentCredentials(for: protectionSpace)

            webView.load(URLRequest(url: configuration.address))
            try await delegate.waitForDidFinishNavigation()
        }

        #expect(!challenges.isEmpty)
        for challenge in challenges {
            #expect(challenge.proposedCredential == nil)
            #expect(challenge.previousFailureCount == 0)
            #expect(challenge.protectionSpace.realm == "testrealm")
            #expect(!challenge.protectionSpace.receivesCredentialSecurely)
            #expect(!challenge.protectionSpace.isProxy())
            #expect(challenge.protectionSpace.host == "127.0.0.1")
            #expect(challenge.protectionSpace.proxyType == nil)
            #expect(challenge.protectionSpace.protocol == NSURLProtectionSpaceHTTP)
            #expect(challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodHTTPBasic)
            #expect((challenge.failureResponse as? HTTPURLResponse)?.statusCode == 401)
        }
    }

    @Test
    func deallocateDuringChallenge() async throws {
        var server = try HTTPServer(protocol: .https) {
            Route("/") {
                "hi"
            }
        }

        let firstChallenge = Future()
        let delegate = TestNavigationDelegate()
        delegate.didReceiveAuthenticationChallenge = { _, challenge, completionHandler in
            firstChallenge.signal()
            completionHandler(.useCredential, challenge.protectionSpace.serverTrust.map(URLCredential.init(trust:)))
        }

        try await server.run { configuration in
            // The pool makes sure nothing autoreleased while starting the loads keeps a view alive once it is removed.
            var views: [WKWebView] = []
            autoreleasepool {
                for _ in 0..<100 {
                    let view = WKWebView()
                    view.navigationDelegate = delegate
                    view.load(URLRequest(url: configuration.address))
                    views.append(view)
                }
            }

            // Deallocate the views once the first TLS handshake reaches its challenge, while the others are still in progress.
            await firstChallenge.wait()
            views.removeAll()
            try await Task.sleep(for: .seconds(1))
        }
    }

    #if HAVE_SEC_KEY_PROXY
    @Test
    func clientCertificate() async throws {
        var server = try HTTPServer(protocol: .https, requiresClientCertificate: true) {
            Route("/") {
                "hello"
            }
        }

        let webView = WKWebView()
        let delegate = TestNavigationDelegate()
        var authenticationMethods: [String] = []
        delegate.didReceiveAuthenticationChallenge = { _, challenge, completionHandler in
            authenticationMethods.append(challenge.protectionSpace.authenticationMethod)

            if challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust {
                completionHandler(.useCredential, challenge.protectionSpace.serverTrust.map(URLCredential.init(trust:)))
                return
            }

            completionHandler(
                .useCredential,
                URLCredential(identity: TestCertificates.identity, certificates: [TestCertificates.certificate], persistence: .none)
            )
        }
        webView.navigationDelegate = delegate

        try await server.run { configuration in
            webView.load(URLRequest(url: configuration.address))
            try await delegate.waitForDidFinishNavigation()
        }

        #expect(authenticationMethods == [NSURLAuthenticationMethodServerTrust, NSURLAuthenticationMethodClientCertificate])
    }
    #endif // HAVE_SEC_KEY_PROXY

    #if hasAttribute(diagnose)
    @diagnose(DeprecatedDeclaration, as: ignored, reason: "Clearing permanent credentials is only exposed on WKProcessPool")
    #endif
    @Test
    func basicProposedCredential() async throws {
        var server = try HTTPServer(protocol: .http, connectionHandler: HTTPServer.respondWithChallengeThenOK)
        let viewConfiguration = WKWebViewConfiguration._test_configurationWithTestPlugInClassName("BasicProposedCredentialPlugIn")
        let webView = WKWebView(frame: .zero, configuration: viewConfiguration)
        let delegate = TestNavigationDelegate()
        var firstChallenge = true
        var receivedSecondChallenge = false
        delegate.didReceiveAuthenticationChallenge = { _, _, completionHandler in
            if firstChallenge {
                firstChallenge = false
                completionHandler(.useCredential, URLCredential(user: "testuser", password: "testpassword", persistence: .permanent))
                return
            }
            receivedSecondChallenge = true
            completionHandler(.useCredential, nil)
        }
        webView.navigationDelegate = delegate

        try await server.run { configuration in
            // Make sure no credential left by previous tests.
            let protectionSpace = URLProtectionSpace(
                host: "127.0.0.1",
                port: configuration.port,
                protocol: NSURLProtectionSpaceHTTP,
                realm: "testrealm",
                authenticationMethod: NSURLAuthenticationMethodHTTPBasic
            )
            webView.configuration.processPool._clearPermanentCredentials(for: protectionSpace)

            // Clear persistent credentials created by this test, even if it fails partway through.
            defer {
                webView.configuration.processPool._clearPermanentCredentials(for: protectionSpace)
            }

            let request = URLRequest(url: configuration.address)
            webView.load(request)
            try await delegate.waitForDidFinishNavigation()
            webView.load(request)
            try await delegate.waitForDidFinishNavigation()
            #expect(receivedSecondChallenge)
        }
    }

    @Test
    func basicPersistentCredential() async throws {
        var server = try HTTPServer(protocol: .http, connectionHandler: HTTPServer.respondWithChallengeThenOK)
        let delegate = TestNavigationDelegate()
        let credentialStorage = URLCredentialStorage.shared
        var challenges: [(challenge: URLAuthenticationChallenge, existingCredential: URLCredential?)] = []
        delegate.didReceiveAuthenticationChallenge = { _, challenge, completionHandler in
            challenges.append((challenge, credentialStorage.defaultCredential(for: challenge.protectionSpace)))
            completionHandler(.useCredential, URLCredential(user: "testuser", password: "testpassword", persistence: .permanent))
        }
        let webView = WKWebView()
        webView.navigationDelegate = delegate

        try await server.run { configuration in
            let protectionSpace = URLProtectionSpace(
                host: "127.0.0.1",
                port: configuration.port,
                protocol: NSURLProtectionSpaceHTTP,
                realm: "testrealm",
                authenticationMethod: NSURLAuthenticationMethodHTTPBasic
            )

            // Remove the credential this test stores, even if it fails partway through.
            defer {
                if let credential = credentialStorage.defaultCredential(for: protectionSpace) {
                    credentialStorage.remove(credential, for: protectionSpace)
                }
            }

            webView.load(URLRequest(url: configuration.address))
            try await delegate.waitForDidFinishNavigation()

            #expect(!challenges.isEmpty)
            for (challenge, existingCredential) in challenges {
                #expect(existingCredential == nil)
                #expect(challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodHTTPBasic)
            }

            try await waitForCondition("the credential to be stored") {
                credentialStorage.defaultCredential(for: protectionSpace) != nil
            }
            let storedCredential = try #require(credentialStorage.defaultCredential(for: protectionSpace))
            #expect(storedCredential.user == "testuser")
            #expect(storedCredential.password == "testpassword")
            #expect(storedCredential.persistence == .permanent)

            credentialStorage.remove(storedCredential, for: protectionSpace)
            let removedCredential = credentialStorage.defaultCredential(for: protectionSpace)
            #expect(removedCredential == nil)
        }
    }

    @Test
    func serverTrust() async throws {
        var server = try HTTPServer(protocol: .https) {
            Route("/") {
                "<script>alert('success!')</script>"
            }
        }

        let webView = WKWebView()
        let delegate = TestNavigationDelegate()
        var challenges: [URLAuthenticationChallenge] = []
        delegate.didReceiveAuthenticationChallenge = { _, challenge, completionHandler in
            challenges.append(challenge)
            completionHandler(.useCredential, challenge.protectionSpace.serverTrust.map(URLCredential.init(trust:)))
        }
        webView.navigationDelegate = delegate

        try await server.run { configuration in
            webView.load(URLRequest(url: configuration.localhostAddress))
            try await delegate.waitForDidFinishNavigation()
        }

        for challenge in challenges {
            #expect(challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust)
            #expect(TestCertificates.verifyCertificateAndPublicKey(of: challenge.protectionSpace.serverTrust))
        }

        #expect(TestCertificates.verifyCertificateAndPublicKey(of: webView.serverTrust))
        #expect(challenges.count > 0)
    }

    @Test
    func fastServerTrust() async throws {
        var server = try HTTPServer(protocol: .https) {
            Route("/") {
                "<script>alert('success!')</script>"
            }
        }

        let viewConfiguration = WKWebViewConfiguration()
        let dataStoreConfiguration = _WKWebsiteDataStoreConfiguration()
        dataStoreConfiguration.fastServerTrustEvaluationEnabled = true
        viewConfiguration.websiteDataStore = WKWebsiteDataStore._store(with: dataStoreConfiguration)
        let webView = WKWebView(frame: .zero, configuration: viewConfiguration)
        let delegate = TestNavigationDelegate()
        var challenges: [URLAuthenticationChallenge] = []
        delegate.didReceiveAuthenticationChallenge = { _, challenge, completionHandler in
            challenges.append(challenge)
            completionHandler(.useCredential, challenge.protectionSpace.serverTrust.map(URLCredential.init(trust:)))
        }
        webView.navigationDelegate = delegate

        try await server.run { configuration in
            webView.load(URLRequest(url: configuration.localhostAddress))
            try await delegate.waitForDidFinishNavigation()
        }

        for challenge in challenges {
            #expect(challenge.protectionSpace.authenticationMethod == NSURLAuthenticationMethodServerTrust)
            #expect(TestCertificates.verifyCertificateAndPublicKey(of: challenge.protectionSpace.serverTrust))
        }

        #expect(challenges.count > 0)
    }

    @Test
    func errorSecureCoding() async throws {
        var terminatesConnection = true
        var server = try HTTPServer(protocol: .http) { connection in
            while try await connection.receiveRequestPath() != nil {
                if terminatesConnection {
                    await connection.terminate()
                    return
                }

                let body = "<script>alert('reloaded successfully')</script>"
                try await connection.send("HTTP/1.1 200 OK\r\nContent-Length: \(body.utf8.count)\r\n\r\n\(body)")
            }
        }

        let webView = WKWebView()
        let delegate = TestNavigationDelegate()
        let alerts = AlertRecorder()
        webView.navigationDelegate = delegate
        webView.uiDelegate = alerts

        try await server.run { configuration in
            webView.load(URLRequest(url: configuration.address))
            let error = try #require(await #expect(throws: NSError.self) { try await delegate.waitForDidFinishNavigation() })

            let recoveryAttempter = try #require(error.userInfo[_WKRecoveryAttempterErrorKey] as? any _WKErrorRecoveryAttempting)
            #expect(NSStringFromClass(type(of: recoveryAttempter)) == "WKReloadFrameErrorRecoveryAttempter")
            let archiver = NSKeyedArchiver(requiringSecureCoding: true)
            archiver.encode(error, forKey: NSKeyedArchiveRootObjectKey)
            archiver.finishEncoding()
            let unarchiver = try NSKeyedUnarchiver(forReadingFrom: archiver.encodedData)
            let recoveryAttempterClass: AnyClass = try #require(NSClassFromString("WKReloadFrameErrorRecoveryAttempter"))
            let decodedObject = unarchiver.decodeObject(
                of: [NSDictionary.self, NSString.self, NSError.self, recoveryAttempterClass],
                forKey: NSKeyedArchiveRootObjectKey
            )
            let decodedError = try #require(decodedObject as? NSError)
            #expect(decodedError.code == NSURLErrorNetworkConnectionLost)
            #expect(decodedError.domain == NSURLErrorDomain)
            let decodedRecoveryAttempter = try #require(
                decodedError.userInfo[_WKRecoveryAttempterErrorKey] as? any _WKErrorRecoveryAttempting
            )
            #expect(NSStringFromClass(type(of: decodedRecoveryAttempter)) == "WKReloadFrameErrorRecoveryAttempter")

            terminatesConnection = false
            #expect(recoveryAttempter.attemptRecovery())
            let alert = try await alerts.nextAlert()
            #expect(alert == "reloaded successfully")
        }
    }
}
