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
import Synchronization
import WebCore_Private.Core.Cookie
import WebKit_Internal
import WebKit_Private
import os
import pal.Core.cocoa.AppSSOSoftLink
import wtf

// One AppSSO authorization for an intercepted navigation or pop-up, and what to load once it answers.
// A session will only be executed once.
@MainActor
final class SOAuthorizationSession {
    // When the web view, owner of the page, is not in the window, a navigation session (redirect or subframe) will proceed
    // through authorization hints and policy decision, then pause before beginning authorization.
    // It will resume after the web view is moved into the window.
    // FSM: Idle => start() => Active => decidePolicyForSOAuthorizationLoad => isInWindow => beginAuthorization => Completed
    //      Idle => start() => Active => decidePolicyForSOAuthorizationLoad => !isInWindow => Waiting => become isInWindow => beginAuthorization => Completed
    // The pop-up session's FSM: Idle => Active => Completed
    enum State: String {
        case idle = "Idle"
        case active = "Active"
        case waiting = "Waiting"
        case completed = "Completed"
    }

    enum Kind {
        // Main frame navigations: the callback cancels (true) or continues (false) the navigation.
        case redirect(callback: WebKit.SOAuthorizationNavigationCallback, windowWait: WindowWait = WindowWait())
        // Subframe navigations: the callback cancels the frame's navigation, whose loads the session then makes.
        case subFrame(
            callback: WebKit.SOAuthorizationNavigationCallback,
            windowWait: WindowWait = WindowWait(),
            loads: SubFrameLoads = SubFrameLoads()
        )
        // Pop-ups: the opener gets a page from the new page callback, or the UI client opens the window.
        case popUp(
            configuration: API.PageConfiguration,
            newPageCallback: WebKit.SOAuthorizationNewPageCallback,
            uiClientCallback: WebKit.SOAuthorizationUIClientCallback,
            secret: SecretWebView = SecretWebView()
        )

        var windowWait: WindowWait? {
            switch self {
            case .redirect(_, let windowWait), .subFrame(_, let windowWait, _):
                windowWait
            case .popUp:
                nil
            }
        }

        var initiatingActionName: String {
            switch self {
            case .redirect:
                "Redirect"
            case .subFrame:
                "SubFrame"
            case .popUp:
                "PopUp"
            }
        }

        var initiatingActionValue: Int {
            switch self {
            case .redirect:
                0
            case .popUp:
                1
            case .subFrame:
                2
            }
        }

        var isSubFrame: Bool {
            if case .subFrame = self {
                return true
            }
            return false
        }
    }

    // A navigation session's pause until its web view is in a window: the observer that resumes it, and the page's
    // active URL when it paused.
    final class WindowWait {
        fileprivate struct Pause {
            let observer: WebKit.SOAuthorizationDidMoveToWindowObserver
            let pageActiveURL: WTF.URL
        }

        fileprivate var pause: Pause?
    }

    private let soAuthorization: SOAuthorization
    let navigationAction: API.NavigationAction
    let page: WebKit.WeakPtrWebPageProxy
    let kind: Kind
    private(set) var state = State.idle

    var presentation = Presentation()

    init(
        _ kind: Kind,
        navigationAction: API.NavigationAction,
        page: WebKit.WebPageProxy,
        authorizationClass: SOAuthorization.Type,
        delegate: any SOAuthorizationDelegate
    ) {
        soAuthorization = authorizationClass.init()
        self.navigationAction = navigationAction
        // FIXME: Drop the .Yes, WeakPtr's default, when WebKit no longer builds its Swift with Swift 6.4.
        self.page = WebKit.WeakPtrWebPageProxy(page, .Yes)
        self.kind = kind
        soAuthorization.delegate = delegate
        if case .subFrame(_, _, let loads) = kind {
            observeFrameLoads(loads)
        }
    }

    isolated deinit {
        switch kind {
        case .redirect(let callback, let windowWait):
            finishNavigation(callback, windowWait: windowWait)
        case .subFrame(let callback, let windowWait, let loads):
            stopObservingFrameLoads(loads)
            finishNavigation(callback, windowWait: windowWait)
        case .popUp(_, let newPageCallback, _, _):
            assert(state != .waiting)
            if newPageCallback.pointee.__convertToBool() {
                newPageCallback.pointee(consuming: WebKit.RefPtrWebPageProxy())
            }
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): viewController=\((self.presentation.viewController as AnyObject?).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )
        #if WTF_PLATFORM_MAC
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): sheet.window=\((self.presentation.sheet?.window).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )
        #endif

        if state == .active {
            soAuthorization.cancel()
        }
        if state != .idle && state != .completed {
            becomeCompleted(deferringWhileHidden: false)
        } else {
            dismissViewController(deferringWhileHidden: false)
        }
    }

    private func finishNavigation(_ callback: WebKit.SOAuthorizationNavigationCallback, windowWait: WindowWait) {
        if callback.pointee.__convertToBool() {
            callback.pointee(true)
        }
        if let observer = windowWait.pause?.observer, let page = page.get() {
            observer.remove(from: page)
        }
    }

    func isForAuthorization(_ authorization: SOAuthorization) -> Bool {
        soAuthorization === authorization
    }

    private func becomeCompleted(deferringWhileHidden: Bool = true) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): viewController=\((self.presentation.viewController as AnyObject?).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )

        assert(state == .active || state == .waiting)
        state = .completed
        dismissViewController(deferringWhileHidden: deferringWhileHidden)
    }

    // Probably not start immediately.
    func shouldStart() {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): page=\(self.page.get().map { $0.logIdentifier() } ?? 0, privacy: .public)"
            )

        assert(state == .idle)
        guard page.get() != nil else {
            return
        }
        switch kind {
        case .redirect:
            break
        case .subFrame(let callback, _, let loads):
            prepareSubFrameStart(callback: callback, loads: loads)
        case .popUp:
            assert(page.get()?.isInWindow() == true)
        }

        start()
    }

    private func start() {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")

        assert(state == .idle || state == .waiting)
        state = .active
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Moving state to Active."
            )
        guard let url = navigationAction.appSSORequestNSURL else {
            return
        }

        Task.immediateOnMainActor { [weak self] in
            let answer = await withCheckedContinuation { continuation in
                let reply = AuthorizationHintsReply(continuation)
                // The task holds the session weakly, so neither it nor its SOAuthorization is kept alive by the wait.
                guard let soAuthorization = self?.soAuthorization else {
                    reply.resume(returning: nil)
                    return
                }

                soAuthorization.getHintsWith(url, responseCode: 0) { @Sendable authorizationHints, error in
                    // Not `!= nil`: unoptimized, that generic call needs the class's metadata, which links AppSSO.
                    let hasHints = if case .some = authorizationHints { true } else { false }
                    reply.resume(
                        returning: AuthorizationHintsAnswer(
                            errorCode: (error as NSError?)?.code,
                            hasHints: hasHints,
                            extensionName: authorizationHints?.localizedExtensionBundleDisplayName
                        )
                    )
                }
            }

            guard let self, let answer else {
                WebKit.LogChannel.AppSSO().releaseLog.error("\(#function, privacy: .public): Returning early because self is now nil.")
                return
            }

            self.didGetAuthorizationHints(answer)
        }
    }

    private func didGetAuthorizationHints(_ answer: AuthorizationHintsAnswer) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Receive SOAuthorizationHints (error=\(answer.errorCode ?? 0, privacy: .public))"
            )

        guard answer.errorCode == nil && answer.hasHints else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Falling back to web path due to error or lack of hints."
                )
            fallBackToWebPath()
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Receive SOAuthorizationHints."
            )
        continueStartAfterGetAuthorizationHints(answer.extensionName)
    }

    // What the hints wait resumes with, read from AppSSO's answer on the queue it answers on.
    private struct AuthorizationHintsAnswer {
        let errorCode: Int?
        let hasHints: Bool
        let extensionName: String?
    }

    // Resumes the hints wait once, with AppSSO's first answer; AppSSO may answer on any queue, and a later answer is
    // logged and ignored. AppSSO's async `hints(with:responseCode:)` returns nonnull hints where the completion passes
    // nullable ones, and a second answer would trap in it.
    private final class AuthorizationHintsReply: Sendable {
        private let continuation: Mutex<CheckedContinuation<AuthorizationHintsAnswer?, Never>?>

        init(_ continuation: CheckedContinuation<AuthorizationHintsAnswer?, Never>) {
            self.continuation = Mutex(continuation)
        }

        func resume(returning answer: AuthorizationHintsAnswer?) {
            guard let continuation = continuation.withLock({ $0.take() }) else {
                WebKit.LogChannel.AppSSO().releaseLog.error("\(#function, privacy: .public): Ignoring an answer after the first.")
                return
            }

            continuation.resume(returning: answer)
        }
    }

    private func continueStartAfterGetAuthorizationHints(_ extensionName: String?) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): hints=\(extensionName ?? "", privacy: .private)"
            )

        assert(state == .active)
        guard let page = page.get() else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Early return due to nil page"
                )
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Checking page for policy choice."
            )

        // See SOAuthorizationSupport::decidePolicy.
        #if compiler(<6.4)
        WebKit.SOAuthorizationSupport.decidePolicy(page, extensionName.map { WTF.String($0 as CFString) } ?? WTF.String()) {
            [weak self] policy in
            self?.continueStartAfterDecidePolicy(policy)
        }
        #else
        page.decidePolicyForSOAuthorizationLoad(
            extensionName.map { WTF.String($0 as CFString) } ?? WTF.String(),
            consuming: .init { [weak self] policy in
                self?.continueStartAfterDecidePolicy(policy)
            }
        )
        #endif
    }

    private func continueStartAfterDecidePolicy(_ policy: WebKit.SOAuthorizationLoadPolicy) {
        guard policy != .Ignore else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Receive SOAuthorizationLoadPolicy.Ignore. Falling back to web path."
                )
            fallBackToWebPath()
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Receive SOAuthorizationLoadPolicy.Allow"
            )

        switch kind {
        case .redirect(_, let windowWait), .subFrame(_, let windowWait, _):
            beginNavigationAuthorizationIfReady(windowWait)
        case .popUp:
            beginAuthorizationIfReady()
        }
    }

    private func beginNavigationAuthorizationIfReady(_ windowWait: WindowWait) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): page=\(self.page.get().map { $0.logIdentifier() } ?? 0, privacy: .public)"
            )

        guard let page = page.get() else {
            return
        }
        guard page.isInWindow() else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Web view is not attached to a window. Pausing authorization until a window is attached."
                )

            state = .waiting
            let observer = WebKit.SOAuthorizationDidMoveToWindowObserver { [weak self] in
                self?.webViewDidMoveToWindow()
            }
            observer.add(to: page)
            assert(page.mainFrame() != nil)
            windowWait.pause = WindowWait.Pause(observer: observer, pageActiveURL: page.activeURLCopy())
            return
        }

        beginAuthorizationIfReady()
    }

    private func webViewDidMoveToWindow() {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")

        guard state == .waiting, let page = page.get(), page.isInWindow(), let pause = kind.windowWait?.pause.take() else {
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Checking whether the page's active URL changed during waiting."
            )
        // FIXME: (rdar://170847912) Use the `!=` operator instead when possible.
        if !(page.activeURLCopy() == pause.pageActiveURL) {
            abort()
            pause.observer.remove(from: page)
            return
        }

        pause.observer.remove(from: page)
        state = .active
        beginAuthorizationIfReady()
    }

    // A pop-up gives its navigation action to the UI client when it falls back.
    private var heldNavigationAction: API.NavigationAction? {
        if case .popUp(_, _, let uiClientCallback, _) = kind, !uiClientCallback.pointee.__convertToBool() {
            return nil
        }

        return navigationAction
    }

    private func beginAuthorizationIfReady() {
        guard let page = page.get(), let navigationAction = heldNavigationAction else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Early return page=\(self.page.get().map { $0.logIdentifier() } ?? 0, privacy: .public)."
                )
            return
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Beginning authorization with AppSSO."
            )
        beginAuthorization(page: page, navigationAction: navigationAction)
    }

    private func beginAuthorization(page: WebKit.WebPageProxy, navigationAction: API.NavigationAction) {
        let mainFrameURL = page.mainFrame()?.urlCopy()
        let mainFrameOrigin = mainFrameURL.map { WebCore.SecurityOrigin.create($0) }

        let initiatorOrigin =
            if let mainFrameOrigin, kind.isSubFrame || !mainFrameOrigin.ptr().isOpaque() {
                mainFrameOrigin.ptr().toString().description
            } else {
                ""
            }
        let initiatingPath = mainFrameURL.map { $0.path().toString().description } ?? ""

        // PAL's getters return String!, which as a dictionary key becomes Optional<String>, a key NSDictionary never finds: each is bound as a String.
        let userActionInitiatedKey: String = PAL.get_AppSSO_SOAuthorizationOptionUserActionInitiatedSingleton()
        let initiatorOriginKey: String = PAL.get_AppSSO_SOAuthorizationOptionInitiatorOriginSingleton()
        let initiatingActionKey: String = PAL.get_AppSSO_SOAuthorizationOptionInitiatingActionSingleton()

        var authorizationOptions: [String: Any] = [
            userActionInitiatedKey: NSNumber(value: navigationAction.isProcessingUserGesture()),
            initiatorOriginKey: initiatorOrigin,
            initiatingActionKey: NSNumber(value: kind.initiatingActionValue),
            kSOAuthorizationOptionInitiatingPath: initiatingPath,
        ]
        #if WTF_PLATFORM_IOS_FAMILY
        if let webView = page.cocoaView().get(),
            let callerSceneID = (webView.uiDelegate as AnyObject?)?._hostSceneIdentifier?(for: webView)
        {
            authorizationOptions["callerSceneIdentifier"] = callerSceneID
        }
        #endif

        soAuthorization.authorizationOptions = authorizationOptions
        #if WTF_PLATFORM_VISION
        // rdar://130904577 - Investigate supporting embedded authorization view controller on visionOS.
        let uiDelegate = page.cocoaView().get()?.uiDelegate
        if uiDelegate?.responds(to: NSSelectorFromString("_presentingViewControllerForWebView:")) != true
            || WTF.IOSApplication.isSafariViewService()
        {
            soAuthorization.enableEmbeddedAuthorizationViewController = false
        }
        page.dispatchWillPresentModalUI()
        #endif

        let request = navigationAction.requestNSURLRequestCopy()
        (soAuthorization as AnyObject).beginAuthorization?(
            with: request?.url,
            httpHeaders: request?.allHTTPHeaderFields,
            httpBody: request?.httpBody
        )
    }

    func resumeWebPath() {
        switch kind {
        case .redirect(let callback, _):
            WebKit.LogChannel.AppSSO().releaseLog
                .log("\(self, privacy: .public) \(#function, privacy: .public)")
            callback.pointee(false)
        case .subFrame(_, _, let loads):
            fallBackSubFrameToWebPath(loads)
        case .popUp(_, let newPageCallback, let uiClientCallback, _):
            WebKit.LogChannel.AppSSO().releaseLog
                .log("\(self, privacy: .public) \(#function, privacy: .public)")
            WebKit.SOAuthorizationSupport.callUIClientCallback(navigationAction, uiClientCallback, newPageCallback)
        }
    }

    // The following should only be called by SOAuthorizationDelegate methods.
    func fallBackToWebPath(userCancel: Bool = false) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")

        guard state == .active else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Returning early since not active."
                )
            dismissViewController()
            return
        }

        becomeCompleted()
        if userCancel, case .subFrame(_, _, let loads) = kind {
            userCancelSubFrame(loads)
            return
        }

        resumeWebPath()
    }

    func abort() {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): viewController=\((self.presentation.viewController as AnyObject?).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )
        #if WTF_PLATFORM_MAC
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): sheet.window=\((self.presentation.sheet?.window).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )
        #endif

        guard state != .idle && state != .completed else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Returning early since idle or already completed."
                )
            dismissViewController()
            return
        }

        becomeCompleted()
        switch kind {
        case .redirect(let callback, _):
            callback.pointee(true)
        case .subFrame:
            resumeWebPath()
        case .popUp(let configuration, let newPageCallback, _, let secret):
            abortPopUp(configuration: configuration, newPageCallback: newPageCallback, secret: secret)
        }
    }

    // Only responses that meet all of the following requirements will be processed:
    // 1) it has the same origin as the request;
    // 2) it has a status code of 200, 302, 307 (POST only), or 401 (with non-empty body).
    // Otherwise, it falls back to the web path.
    // Only the following HTTP headers will be processed:
    // { Set-Cookie, Location }.
    func complete(_ httpResponse: HTTPURLResponse, body: Data) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): httpState=\(httpResponse.statusCode, privacy: .public), viewController=\((self.presentation.viewController as AnyObject?).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )
        #if WTF_PLATFORM_MAC
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): sheet.window=\((self.presentation.sheet?.window).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )
        #endif

        guard state == .active else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Returning early since not active."
                )
            dismissViewController()
            return
        }

        becomeCompleted()
        // Swift 6.3 imports WebCore::ResourceResponse as an unsafe type, which Swift 6.4 does not.
        // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
        #if compiler(<6.4)
        let response = unsafe WebCore.ResourceResponse(httpResponse)
        let responseURL = unsafe response.urlCopy()
        let statusCode = unsafe response.httpStatusCode()
        let location = unsafe response.httpLocation()
        #else
        let response = WebCore.ResourceResponse(httpResponse)
        let responseURL = response.urlCopy()
        let statusCode = response.httpStatusCode()
        let location = response.httpLocation()
        #endif

        // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
        #if compiler(<6.4)
        if case .subFrame = kind, unsafe shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(response) {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): CSP failed. Falling back to web path."
                )
            resumeWebPath()
            return
        }
        #else
        if case .subFrame = kind, shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(response) {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): CSP failed. Falling back to web path."
                )
            resumeWebPath()
            return
        }
        #endif
        guard isSameOrigin(navigationAction, responseURL) else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Origins don't match. Falling back to web path."
                )
            resumeWebPath()
            return
        }

        // Set cookies.
        let cookies = responseCookies(httpResponse, responseURL)
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): (httpStatusCode=\(statusCode, privacy: .public), hasCookies=\(!cookies.isEmpty, privacy: .public), hasData=\(!body.isEmpty, privacy: .public))"
            )

        guard !cookies.isEmpty else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): No cookies to set. Completing (internal)."
                )
            // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
            #if compiler(<6.4)
            unsafe completeAfterSettingCookies(
                httpResponse,
                response: response,
                responseURL: responseURL,
                statusCode: statusCode,
                location: location,
                body: body
            )
            #else
            completeAfterSettingCookies(
                httpResponse,
                response: response,
                responseURL: responseURL,
                statusCode: statusCode,
                location: location,
                body: body
            )
            #endif
            return
        }
        guard let page = page.get() else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Returning early because page is nil."
                )
            return
        }

        // https://bugs.webkit.org/show_bug.cgi?id=299083
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Setting \(cookies.count, privacy: .public) cookies with total header size ~\(cookies.reduce(0) { $0 + $1.name.utf16.count + $1.value.utf16.count + 1 }, privacy: .public) bytes in data store \(page.websiteDataStore().sessionID().toUInt64(), privacy: .public)"
            )

        // See SOAuthorizationSupport::setCookies.
        #if compiler(<6.4)
        unsafe WebKit.SOAuthorizationSupport.setCookies(page, cookies) { [weak self, response, responseURL, location] in
            guard let self else {
                return
            }

            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Cookies are set."
                )
            unsafe completeAfterSettingCookies(
                httpResponse,
                response: response,
                responseURL: responseURL,
                statusCode: statusCode,
                location: location,
                body: body
            )
        }
        #else
        var cookieVector = WebKit.SOAuthorizationCookieVector()
        cookieVector.reserveCapacity(cookies.count)
        for cookie in cookies {
            cookieVector.append(consuming: WebCore.Cookie(cookie))
        }

        let cookieStore = page.websiteDataStore().cookieStore()
        cookieStore.setCookies(
            consuming: cookieVector,
            consuming: .init { [weak self, response, responseURL, location] in
                guard let self else {
                    return
                }

                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): Cookies are set."
                    )
                completeAfterSettingCookies(
                    httpResponse,
                    response: response,
                    responseURL: responseURL,
                    statusCode: statusCode,
                    location: location,
                    body: body
                )
            }
        )
        #endif
    }

    private func completeAfterSettingCookies(
        _ httpResponse: HTTPURLResponse,
        response: WebCore.ResourceResponse,
        responseURL: WTF.URL,
        statusCode: Int32,
        location: WTF.String,
        body: Data
    ) {
        switch kind {
        case .redirect(let callback, _):
            // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
            #if compiler(<6.4)
            unsafe completeRedirect(
                httpResponse,
                response: response,
                responseURL: responseURL,
                statusCode: statusCode,
                location: location,
                body: body,
                callback: callback
            )
            #else
            completeRedirect(
                httpResponse,
                response: response,
                responseURL: responseURL,
                statusCode: statusCode,
                location: location,
                body: body,
                callback: callback
            )
            #endif
        case .subFrame(_, _, let loads):
            completeSubFrame(httpResponse, body: body, responseURL: responseURL, loads: loads)
        case .popUp(let configuration, let newPageCallback, _, let secret):
            completePopUp(httpResponse, body: body, configuration: configuration, newPageCallback: newPageCallback, secret: secret)
        }
    }

    private func completeRedirect(
        _ httpResponse: HTTPURLResponse,
        response: WebCore.ResourceResponse,
        responseURL: WTF.URL,
        statusCode: Int32,
        location: WTF.String,
        body: Data,
        callback: WebKit.SOAuthorizationNavigationCallback
    ) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): httpState=\(httpResponse.statusCode, privacy: .public)"
            )

        guard let page = page.get() else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): httpState=\(httpResponse.statusCode, privacy: .public) page=false, so falling back to web path."
                )
            resumeWebPath()
            return
        }

        switch statusCode {
        case WebCore.httpStatus302Found:
            callback.pointee(true)
            loadRedirectLocation(page, location: location)
        case WebCore.httpStatus200OK:
            callback.pointee(true)
            loadResponseBody(page, url: responseURL, body: body)
        case WebCore.httpStatus307TemporaryRedirect where navigationAction.requestIsPOST():
            // FIXME: Enable the useRedirectionForCurrentNavigation code path for all redirections.
            // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
            #if compiler(<6.4)
            unsafe page.useRedirectionForCurrentNavigation(response)
            #else
            page.useRedirectionForCurrentNavigation(response)
            #endif
            callback.pointee(false)
        case WebCore.httpStatus401Unauthorized where !body.isEmpty:
            // When the SSO extension completes with a 401 response and non-empty body data,
            // load the response body so the page can continue the authentication flow.
            // The extension chose didCompleteWithHTTPResponse: over authorizationDidNotHandle:,
            // indicating it has meaningful content for the browser to render. rdar://145336725
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): loading 401 response body (\(body.count, privacy: .public) bytes)"
                )
            callback.pointee(true)
            loadResponseBody(page, url: responseURL, body: body)
        default:
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): httpState=\(httpResponse.statusCode, privacy: .public), so falling back to web path."
                )
            resumeWebPath()
        }
    }

    private func loadRedirectLocation(_ page: WebKit.WebPageProxy, location: WTF.String) {
        #if WTF_PLATFORM_IOS || WTF_PLATFORM_VISION
        // MobileSafari has a WBSURLSpoofingMitigator, which will not display the provisional URL for navigations without user gestures.
        // For slow loads that are initiated from the MobileSafari Favorites screen, the aforementioned behavior will create a period
        // after authentication completion where the new request to the application site loads with a blank URL and blank page. To
        // work around this issue, we load a page that does a client side redirection to the application site on behalf of the
        // request URL, instead of directly loading a new request. This local page should be super fast to load and therefore will not
        // show an empty URL or a blank page. These changes ensure a relevant URL bar and useful page content during the load.
        if !navigationAction.isProcessingUserGesture() {
            page.setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision()
            let html = Data("<script>location = '\(location.description)'</script>".utf8)
            _ = page.loadData(
                consuming: WebCore.SharedBuffer.create(html as NSData),
                WTF.String("text/html" as CFString),
                WTF.String("UTF-8" as CFString),
                navigationAction.requestURLCopy().stringCopy(),
                nil,
                navigationAction.shouldOpenExternalURLsPolicy()
            )
            return
        }
        #endif

        // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
        #if compiler(<6.4)
        _ = unsafe page.loadRequest(consuming: WebCore.ResourceRequest(consuming: location))
        #else
        _ = page.loadRequest(consuming: WebCore.ResourceRequest(consuming: location))
        #endif
    }

    private func loadResponseBody(_ page: WebKit.WebPageProxy, url: WTF.URL, body: Data) {
        page.setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision()
        _ = page.loadData(
            consuming: WebCore.SharedBuffer.create(body as NSData),
            WTF.String("text/html" as CFString),
            WTF.String("UTF-8" as CFString),
            url.stringCopy(),
            nil,
            navigationAction.shouldOpenExternalURLsPolicy()
        )
    }
}

// The session's log prefix.
extension SOAuthorizationSession: @MainActor CustomStringConvertible {
    var description: String {
        "\(String(describing: ObjectIdentifier(self))) - [InitiatingAction=\(kind.initiatingActionName)][State=\(state.rawValue)]"
    }
}

private func isSameOrigin(_ navigationAction: API.NavigationAction, _ responseURL: WTF.URL) -> Bool {
    let requestOrigin = WebCore.SecurityOrigin.create(navigationAction.requestURLCopy())
    return requestOrigin.ptr().isSameOriginAs(WebCore.SecurityOrigin.create(responseURL).ptr())
}

// The response's cookies, from its header fields as AppSSO's response holds them. Foundation takes the fields as strings, so
// a field whose name or value is not a string is left out.
private func responseCookies(_ httpResponse: HTTPURLResponse, _ responseURL: WTF.URL) -> [HTTPCookie] {
    var headerFields: [String: String] = [:]
    for case (let name as String, let value as String) in httpResponse.allHeaderFields {
        headerFields[name] = value
    }

    guard let nsURL = responseURL.appSSONSURL else {
        return []
    }

    return HTTPCookie.cookies(withResponseHeaderFields: headerFields, for: nsURL)
}

// AppSSO's begin call as the C++ sent it: the request's URL, header fields and body, each nil when the request has none.
// AppSSO declares them nonnull, so Swift sends the same message through this declaration, which allows nil.
@objc
private protocol SOAuthorizationBeginWithNullableRequestParts {
    @objc(beginAuthorizationWithURL:httpHeaders:httpBody:)
    func beginAuthorization(with url: URL?, httpHeaders: [String: String]?, httpBody: Data?)
}

extension WebKit.WebPageProxy {
    var isMainFrameURLFromAppleOwnedDomain: Bool {
        guard let url = mainFrame()?.urlCopy().appSSONSURL else {
            return false
        }

        return AKAuthorizationController.isURL(fromAppleOwnedDomain: url)
    }
}

extension WTF.URL {
    fileprivate var appSSONSURL: URL? {
        createNSURL().get()
    }
}

extension API.NavigationAction {
    // FIXME: (rdar://193738426) Remove once Swift can call request(), which returns a reference, without unsafe.
    // The copied request, which owns its NSURLRequest, is kept alive until the bridge to URLRequest has copied it.
    fileprivate borrowing func requestNSURLRequestCopy() -> URLRequest? {
        withExtendedLifetime(requestCopy()) { request in request.nsURLRequest(WebCore.UpdateHTTPBody) }
    }

    var appSSORequestNSURL: URL? {
        requestURLCopy().appSSONSURL
    }
}

#endif
