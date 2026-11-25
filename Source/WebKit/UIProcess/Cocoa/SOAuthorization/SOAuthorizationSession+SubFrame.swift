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
import WebCore_Private.Core.HTTPParsers
import WebKit_Internal
import os
import wtf

extension SOAuthorizationSession {
    // A load the subframe session makes in its frame: the data to load at the URL, or the URL itself with its referrer.
    struct SubFrameLoad {
        enum Content {
            case data(Data)
            case referrer(WTF.String)
        }

        let url: WTF.URL
        let content: Content
    }

    // The loads a subframe session makes in its frame, one at a time, and the observer that tells it when each finishes.
    final class SubFrameLoads {
        fileprivate var observer: WebKit.SOAuthorizationFrameLoadObserver?
        fileprivate var queue: [SubFrameLoad] = []
    }

    private static let postDidStartMessageToParent = Data("<script>parent.postMessage('SOAuthorizationDidStart', '*');</script>".utf8)
    private static let postDidCancelMessageToParent = Data("<script>parent.postMessage('SOAuthorizationDidCancel', '*');</script>".utf8)
    private static let postDidUserCancelMessageToParent = Data(
        "<script>parent.postMessage('SOAuthorizationDidUserCancel', '*');</script>".utf8
    )

    // Observes the target frame's loads, so that each finished load starts the next one.
    func observeFrameLoads(_ loads: SubFrameLoads) {
        guard let frame = targetWebFrame else {
            return
        }

        let observer = WebKit.SOAuthorizationFrameLoadObserver { [weak self, weak loads] in
            guard let self, let loads else {
                return
            }
            didFinishLoad(loads)
        }

        observer.add(to: frame)
        loads.observer = observer
    }

    func stopObservingFrameLoads(_ loads: SubFrameLoads) {
        if let observer = loads.observer, let frame = targetWebFrame {
            observer.remove(from: frame)
        }
    }

    func prepareSubFrameStart(callback: WebKit.SOAuthorizationNavigationCallback, loads: SubFrameLoads) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")

        // Cancelled the current load before loading the data to post SOAuthorizationDidStart to the parent frame.
        callback.pointee(true)
        appendRequestToLoad(
            navigationAction.requestURLCopy(),
            .data(Self.postDidStartMessageToParent),
            in: loads
        )
    }

    func userCancelSubFrame(_ loads: SubFrameLoads) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")
        appendMessageAndOriginalRequest(Self.postDidUserCancelMessageToParent, in: loads)
    }

    func fallBackSubFrameToWebPath(_ loads: SubFrameLoads) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")
        appendMessageAndOriginalRequest(Self.postDidCancelMessageToParent, in: loads)
    }

    func completeSubFrame(_ httpResponse: HTTPURLResponse, body: Data, responseURL: WTF.URL, loads: SubFrameLoads) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public): httpState=\(httpResponse.statusCode, privacy: .public)")
        guard httpResponse.statusCode == Int(WebCore.httpStatus200OK) else {
            resumeWebPath()
            return
        }

        appendRequestToLoad(responseURL, .data(body), in: loads)
    }

    private func appendMessageAndOriginalRequest(_ message: Data, in loads: SubFrameLoads) {
        let url = navigationAction.requestURLCopy()
        appendRequestToLoad(url, .data(message), in: loads)
        appendRequestToLoad(url, .referrer(navigationAction.requestCopy().httpReferrer()), in: loads)
    }

    private func appendRequestToLoad(_ url: WTF.URL, _ content: SubFrameLoad.Content, in loads: SubFrameLoads) {
        loads.queue.append(SubFrameLoad(url: url, content: content))
        if loads.queue.count == 1 {
            loadRequestToFrame(loads)
        }
    }

    private func didFinishLoad(_ loads: SubFrameLoads) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")
        guard let frame = targetWebFrame, let first = loads.queue.first, frame.urlCopy() == first.url else {
            return
        }

        loads.queue.removeFirst()
        loadRequestToFrame(loads)
    }

    private func loadRequestToFrame(_ loads: SubFrameLoads) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log("\(self, privacy: .public) \(#function, privacy: .public)")

        guard let page = page.get(), let first = loads.queue.first else {
            return
        }

        let frame = targetWebFrame
        assert(frame.map { WebKit.SOAuthorizationSupport.isFrameInPage($0, page) } ?? true)
        guard let frame, WebKit.SOAuthorizationSupport.isFrameInPage(frame, page) else {
            return
        }

        page.setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision()
        switch first.content {
        case .data(let data):
            // FIXME: Remove this path when WebKit no longer builds its Swift with Swift 6.4, whose conversion of an
            // empty Span to a std::span traps (rdar://191125558).
            #if compiler(<6.5)
            let oneByte = Data(count: 1)
            let bytes = data.isEmpty ? oneByte.span.extracting(0..<0) : data.span
            #else
            let bytes = data.span
            #endif
            frame.loadData(bytes, WTF.String("text/html" as CFString), WTF.String("UTF-8" as CFString), first.url)
        case .referrer(let referrer):
            frame.loadURL(first.url, referrer)
        }
    }

    private var targetWebFrame: WebKit.WebFrameProxy? {
        navigationAction.targetFrame().flatMap { WebKit.WebFrameProxy.webFrame($0.handle().ptr().frameID().asOptional()) }
    }

    private func shouldInterruptLoadForXFrameOptions(
        _ frameAncestorOrigins: borrowing WebKit.SOAuthorizationOriginVector,
        _ xFrameOptions: WTF.String,
        _ url: WTF.URL
    ) -> Bool {
        switch WebCore.parseXFrameOptionsHeader(WTF.StringView(xFrameOptions)) {
        case .None, .AllowAll:
            return false
        case .Deny:
            return true
        case .SameOrigin:
            let origin = WebCore.SecurityOrigin.create(url)
            #if HAVE_SWIFT_STDLIB_6_4
            for ancestorOrigin in frameAncestorOrigins where !origin.ptr().isSameSchemeHostPort(ancestorOrigin.ptr()) {
                return true
            }
            #else
            // FIXME: Remove this fallback when WebKit no longer supports macOS, iOS and visionOS before 27.
            for index in 0..<frameAncestorOrigins.size() where !origin.ptr().isSameSchemeHostPort(frameAncestorOrigins[index].ptr()) {
                return true
            }
            #endif
            return false
        case .Conflict:
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Multiple 'X-Frame-Options' headers with conflicting values ('\(xFrameOptions.description, privacy: .private)') encountered. Falling back to 'DENY'."
                )
            return true
        case .Invalid:
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Invalid 'X-Frame-Options' header encountered: '\(xFrameOptions.description, privacy: .private)' is not a recognized directive. The header will be ignored."
                )
            return false
        @unknown default:
            assertionFailure()
            return false
        }
    }

    func shouldInterruptLoadForCSPFrameAncestorsOrXFrameOptions(_ response: WebCore.ResourceResponse) -> Bool {
        if let page = page.get(), page.preferences().ignoreIframeEmbeddingProtectionsEnabled() {
            return false
        }

        var frameAncestorOrigins = WebKit.SOAuthorizationOriginVector()
        if let targetFrame = navigationAction.targetFrame(), let parentFrameHandle = targetFrame.parentFrameHandle().get() {
            var parent = WebKit.WebFrameProxy.webFrame(parentFrameHandle.frameID().asOptional())
            while let frame = parent {
                frameAncestorOrigins.append(consuming: WebCore.SecurityOrigin.create(frame.urlCopy()))
                parent = frame.parentFrame()
            }
        }

        // Swift 6.3 imports WebCore::ResourceResponse as an unsafe type, which Swift 6.4 does not.
        // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
        #if compiler(<6.4)
        let url = unsafe response.urlCopy()
        #else
        let url = response.urlCopy()
        #endif

        let decision = WebKit.SOAuthorizationSupport.frameAncestorsDecision(
            response,
            navigationAction.requestCopy().httpReferrer(),
            frameAncestorOrigins
        )

        switch decision {
        case .Interrupt:
            return true
        case .Allow:
            return false
        default:
            break
        }

        // FIXME: Remove this compiler(<6.4) path when WebKit no longer builds its Swift with Swift 6.3.
        #if compiler(<6.4)
        let xFrameOptions = unsafe response.httpXFrameOptions()
        #else
        let xFrameOptions = response.httpXFrameOptions()
        #endif
        guard !xFrameOptions.isNull(), shouldInterruptLoadForXFrameOptions(frameAncestorOrigins, xFrameOptions, url) else {
            return false
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Refused to display '\(url.stringCenterEllipsizedToLength().description, privacy: .private)' in a frame because it set 'X-Frame-Options' to '\(xFrameOptions.description, privacy: .private)'."
            )

        return true
    }
}

#endif
