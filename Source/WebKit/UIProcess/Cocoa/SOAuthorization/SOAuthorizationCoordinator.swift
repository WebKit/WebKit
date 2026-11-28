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

#if HAVE_APP_SSO

import Foundation
import WebKit_Internal
import os
import pal.Core.cocoa.AppSSOSoftLink
import wtf

// AppSSO's SPI types the view controller as id on macOS.
#if WTF_PLATFORM_MAC
typealias SOAuthorizationPresentedViewController = Any
#else
typealias SOAuthorizationPresentedViewController = SOAuthorizationViewController
#endif

// AppSSO's SOAuthorization class through PAL's soft link, which loads the framework first. The Swift must not name the
// class symbol (WebKit does not link AppSSO), so the class comes from its name, as WKRevealItemPresenter.swift does for Reveal.
// It is nil only on macOS, where the framework may be missing; elsewhere a missing class traps, as AppSSOSoftLink.mm's
// SOFT_LINK_CLASS_FOR_SOURCE_WITH_EXPORT does with RELEASE_ASSERT.
private func appSSOAuthorizationClass() -> SOAuthorization.Type? {
    let isAvailable = PAL.isAppSSOFrameworkAvailable()
    // Safety: the name and the type are written statically here, and name the class PAL's AppSSOSoftLink.mm soft-links.
    // C++: SOAuthorizationCoordinator.mm:61 `PAL::getSOAuthorizationClassSingleton()`
    let authorizationClass = isAvailable ? unsafe softLinkedClass(named: "SOAuthorization", as: SOAuthorization.self) : nil
    #if !WTF_PLATFORM_MAC
    precondition(authorizationClass != nil, "AppSSO's SOAuthorization class is missing")
    #endif

    return authorizationClass
}

// FIXME: (rdar://193738752) Swift cannot expose an actor-isolated class to C++, so C++ makes it through this function.
@_expose(Cxx)
func makeSOAuthorizationCoordinatorBlocks() -> WebKit.SOAuthorizationCoordinatorBlocks {
    let coordinator = WKSOAuthorizationCoordinator()
    return WebKit.SOAuthorizationCoordinatorBlocks(
        { navigationAction, page, callback in
            coordinator.tryAuthorize(navigationAction, page: page, callback: callback)
        },
        { configuration, navigationAction, page, newPageCallback, uiClientCallback in
            coordinator.tryAuthorize(
                configuration,
                navigationAction: navigationAction,
                page: page,
                newPageCallback: newPageCallback,
                uiClientCallback: uiClientCallback
            )
        }
    )
}

// Asks AppSSO whether a navigation's URL belongs to a sign-on extension, and hands the navigation to a session if it does.
@MainActor
private final class WKSOAuthorizationCoordinator {
    // AppSSO's SOAuthorization class, and the delegate of every session's authorization.
    private struct AppSSO {
        let authorizationClass: SOAuthorization.Type
        let delegate: WKSOAuthorizationDelegate

        // A property: in an async function, naming this selector warns "consider using asynchronous alternative function".
        var canPerformAsynchronously: Bool {
            authorizationClass.responds(
                to: #selector(
                    SOAuthorization.canPerform(
                        with:
                        responseCode:
                        callerBundleIdentifier:
                        useInternalExtensions:
                        completion:
                    )
                )
            )
        }
    }

    // nil without AppSSO.
    private let appSSO: AppSSO?

    nonisolated init() {
        // In the case of base system, which doesn't have AppSSO.framework.
        guard let authorizationClass = appSSOAuthorizationClass() else {
            appSSO = nil
            return
        }

        appSSO = AppSSO(authorizationClass: authorizationClass, delegate: WKSOAuthorizationDelegate())
        URLSession._disableAppSSO()
    }

    // What a tryAuthorize call does once AppSSO answers canAuthorize.
    private enum Interception {
        case navigation(callback: WebKit.SOAuthorizationNavigationCallback)
        case popUp(
            configuration: API.PageConfiguration,
            newPageCallback: WebKit.SOAuthorizationNewPageCallback,
            uiClientCallback: WebKit.SOAuthorizationUIClientCallback
        )
    }

    // For navigation interception.
    func tryAuthorize(
        _ navigationAction: API.NavigationAction,
        page: WebKit.WebPageProxy,
        callback: WebKit.SOAuthorizationNavigationCallback
    ) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public)"
            )
        tryAuthorize(.navigation(callback: callback), navigationAction: navigationAction, page: page)
    }

    // For pop-up interception.
    func tryAuthorize(
        _ configuration: API.PageConfiguration,
        navigationAction: API.NavigationAction,
        page: WebKit.WebPageProxy,
        newPageCallback: WebKit.SOAuthorizationNewPageCallback,
        uiClientCallback: WebKit.SOAuthorizationUIClientCallback
    ) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public)"
            )

        if let sourceFrame = navigationAction.sourceFrame(), !sourceFrame.isMainFrame() {
            WebKit.LogChannel.AppSSO().releaseLog
                .error(
                    "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Attempting to perform subframe navigation."
                )
            WebKit.SOAuthorizationSupport.callUIClientCallback(navigationAction, uiClientCallback, newPageCallback)
            return
        }
        guard navigationAction.isProcessingUserGesture() else {
            WebKit.LogChannel.AppSSO().releaseLog
                .error(
                    "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Attempting to perform auth without a user gesture."
                )
            WebKit.SOAuthorizationSupport.callUIClientCallback(navigationAction, uiClientCallback, newPageCallback)
            return
        }

        tryAuthorize(
            .popUp(configuration: configuration, newPageCallback: newPageCallback, uiClientCallback: uiClientCallback),
            navigationAction: navigationAction,
            page: page
        )
    }

    // The task holds the page, the navigation action and the callbacks until AppSSO answers, and not the coordinator. It
    // runs on the main thread, as the callbacks are made and called there.
    private func tryAuthorize(_ interception: Interception, navigationAction: API.NavigationAction, page: WebKit.WebPageProxy) {
        let appSSO = appSSO
        Task.immediateOnMainActor {
            guard let appSSO = await Self.canAuthorize(navigationAction.appSSORequestNSURL, appSSO: appSSO) else {
                switch interception {
                case .navigation(let callback):
                    WebKit.LogChannel.AppSSO().releaseLog
                        .log(
                            "\(#function, privacy: .public): The requested URL is not registered for AppSSO handling. No further action needed."
                        )
                    callback.pointee(false)
                case .popUp(_, let newPageCallback, let uiClientCallback):
                    WebKit.LogChannel.AppSSO().releaseLog
                        .error(
                            "\(#function, privacy: .public): Attempting to perform subframe navigation."
                        )
                    WebKit.SOAuthorizationSupport.callUIClientCallback(navigationAction, uiClientCallback, newPageCallback)
                }
                return
            }

            let kind: SOAuthorizationSession.Kind
            switch interception {
            case .navigation(let callback):
                // A subframe session should only be allowed for Apple first parties.
                let isSubframeNavigation = navigationAction.targetFrame().map { !$0.isMainFrame() } ?? false
                if isSubframeNavigation && !page.isMainFrameURLFromAppleOwnedDomain {
                    WebKit.LogChannel.AppSSO().releaseLog
                        .error(
                            "\(#function, privacy: .public): Attempting to perform subframe navigation for non-Apple authorization URL."
                        )
                    callback.pointee(false)
                    return
                }

                kind = isSubframeNavigation ? .subFrame(callback: callback) : .redirect(callback: callback)
            case .popUp(let configuration, let newPageCallback, let uiClientCallback):
                kind = .popUp(configuration: configuration, newPageCallback: newPageCallback, uiClientCallback: uiClientCallback)
            }

            appSSO.delegate.start(
                SOAuthorizationSession(
                    kind,
                    navigationAction: navigationAction,
                    page: page,
                    authorizationClass: appSSO.authorizationClass,
                    delegate: appSSO.delegate
                )
            )
        }
    }

    // Answers with AppSSO if it handles the URL, and nil if it does not.
    private static func canAuthorize(_ url: URL?, appSSO: AppSSO?) async -> AppSSO? {
        guard let appSSO else {
            WebKit.LogChannel.AppSSO().releaseLog.log("\(#function, privacy: .public): No AppSSO framework")
            return nil
        }
        guard let url else {
            return nil
        }

        let authorizationClass = appSSO.authorizationClass
        guard appSSO.canPerformAsynchronously else {
            return authorizationClass.canPerform(with: url, responseCode: 0) ? appSSO : nil
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(#function, privacy: .public): Calling async canPerform(with:responseCode:callerBundleIdentifier:useInternalExtensions:completion:)"
            )
        let result = await authorizationClass.canPerform(
            with: url,
            responseCode: 0,
            callerBundleIdentifier: nil,
            useInternalExtensions: true
        )
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(#function, privacy: .public): Async canPerform(with:responseCode:callerBundleIdentifier:useInternalExtensions:completion:) completed, result=\(result, privacy: .public)"
            )

        return result ? appSSO : nil
    }
}

// MARK: WKSOAuthorizationDelegate

// AppSSO's delegate: the current session gets AppSSO's answers for its authorization.
@MainActor
private final class WKSOAuthorizationDelegate: NSObject, @MainActor SOAuthorizationDelegate {
    private static let authenticationErrorDomain = "AKAuthenticationError" // AKAppleIDAuthenticationErrorDomain
    private static let userCanceled = -7003 // AKAuthenticationErrorUserCanceled

    private var session: SOAuthorizationSession?

    nonisolated override init() {
        super.init()
    }

    func start(_ session: SOAuthorizationSession) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): (existing session = \(self.session.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public), new session = \(String(describing: ObjectIdentifier(session)), privacy: .public))"
            )

        self.session = session
        session.shouldStart()
    }

    // Swift imports this method as authorization(_:presentViewController:) on macOS, where AppSSO types the view
    // controller as id, and as authorization(_:present:) on the iOS family. The view controller is optional, though
    // AppSSO declares it nonnull, so that a nil one completes with false.
    #if WTF_PLATFORM_MAC
    func authorization(
        _ authorization: SOAuthorization,
        presentViewController viewController: SOAuthorizationPresentedViewController?
    ) async throws -> Bool {
        try presentViewController(viewController, for: authorization)
    }
    #else
    func authorization(
        _ authorization: SOAuthorization,
        present viewController: SOAuthorizationPresentedViewController?
    ) async throws -> Bool {
        try presentViewController(viewController, for: authorization)
    }
    #endif

    private func presentViewController(
        _ viewController: SOAuthorizationPresentedViewController?,
        for authorization: SOAuthorization
    ) throws -> Bool {
        guard
            let session = session(
                for: authorization,
                outcome: "completing with false as success state"
            )
        else {
            return false
        }
        guard let viewController else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): No view controller to present, so completing with false as success state."
                )
            return false
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): presentingViewController \(String(describing: ObjectIdentifier(viewController as AnyObject)), privacy: .public)"
            )
        try session.present(viewController)

        return true
    }

    func authorizationDidNotHandle(_ authorization: SOAuthorization) {
        guard let session = session(for: authorization) else {
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Falling back to web path."
            )
        session.fallBackToWebPath()
    }

    func authorizationDidCancel(_ authorization: SOAuthorization) {
        guard let session = session(for: authorization) else {
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Aborting session."
            )
        session.abort()
    }

    func authorizationDidComplete(_ authorization: SOAuthorization) {
        guard let session = session(for: authorization) else {
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Falling back to web path."
            )
        session.fallBackToWebPath()
    }

    func authorization(
        _ authorization: SOAuthorization,
        didCompleteWithHTTPAuthorizationHeaders httpAuthorizationHeaders: [String: String]
    ) {
        guard let session = session(for: authorization) else {
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Falling back to web path."
            )
        session.fallBackToWebPath()
    }

    func authorization(
        _ authorization: SOAuthorization,
        didCompleteWithHTTPResponse httpResponse: HTTPURLResponse,
        httpBody: Data
    ) {
        guard let session = session(for: authorization) else {
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Completing."
            )
        session.complete(httpResponse, body: httpBody)
    }

    func authorization(_ authorization: SOAuthorization, didCompleteWithError error: any Error) {
        guard let session = session(for: authorization) else {
            return
        }

        let nsError = error as NSError
        if nsError.domain == Self.authenticationErrorDomain && nsError.code == Self.userCanceled {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): User cancelled"
                )
            session.fallBackToWebPath(userCancel: true)
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(#function, privacy: .public): Falling back to web path."
            )
        session.fallBackToWebPath()
    }

    // Logs the delegate call, then returns the current session if AppSSO's answer is for its authorization.
    private func session(
        for authorization: SOAuthorization,
        function: String = #function,
        outcome: String = "returning early"
    ) -> SOAuthorizationSession? {
        precondition(WTF.isMainRunLoop())

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(function, privacy: .public): (authorization = \(String(describing: ObjectIdentifier(authorization)), privacy: .public), session = \(self.session.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public))"
            )

        guard let session else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(function, privacy: .public): No session, so \(outcome, privacy: .public)."
                )
            assertionFailure("AppSSO called the delegate with no session")
            return nil
        }
        guard session.isForAuthorization(authorization) else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(String(describing: ObjectIdentifier(self)), privacy: .public) - \(function, privacy: .public): Authorization does not belong to the current session, so \(outcome, privacy: .public)."
                )
            return nil
        }

        return session
    }
}

#endif
