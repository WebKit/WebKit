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
import WebKit_Private
import os
import pal.Core.cocoa.AppSSOSoftLink
import wtf

extension SOAuthorizationSession {
    #if WTF_PLATFORM_MAC
    #if HAVE_APPKIT_MAIN_ACTOR_MESSAGES
    typealias NotificationObserver = NotificationCenter.ObservationToken
    #else
    // FIXME: Remove this fallback when WebKit no longer supports macOS before 27.
    typealias NotificationObserver = any NSObjectProtocol
    #endif

    // The sheet that presents AppSSO's view controller, and the observer that dismisses it when the sheet closes.
    struct Sheet {
        let window: NSWindow
        let willCloseObserver: NotificationObserver
    }
    #endif

    // AppSSO's view controller while the session presents it and, on macOS, its sheet and the observers that put off
    // its dismissal.
    struct Presentation {
        fileprivate(set) var viewController: SOAuthorizationPresentedViewController?
        #if WTF_PLATFORM_MAC
        fileprivate(set) var sheet: Sheet?
        fileprivate(set) var presentingWindowDidDeminiaturizeObserver: NotificationObserver?
        fileprivate(set) var applicationDidUnhideObserver: NotificationObserver?
        #endif
    }

    // Should only be called by SOAuthorizationDelegate methods.
    func present(_ viewController: SOAuthorizationPresentedViewController) throws {
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

        assert(state == .active)
        // Only expect at most one UI session for the whole authorization session.
        guard let page = page.get(), !page.isClosed(), self.presentation.viewController == nil else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): page=\(self.page.get().map { $0.logIdentifier() } ?? 0, privacy: .public), page.isClosed=\(self.page.get()?.isClosed() == true, privacy: .public), viewController=\((self.presentation.viewController as AnyObject?).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
                )
            throw Self.presentationFailedError
        }

        self.presentation.viewController = viewController
        #if WTF_PLATFORM_MAC
        assert(presentation.sheet == nil)
        dismissModalSheetIfNecessary()
        // AppSSO gives an NSViewController on macOS.
        // swift-format-ignore: NeverForceUnwrap
        let sheetWindow = NSWindow(contentViewController: viewController as! NSViewController)

        #if HAVE_APPKIT_MAIN_ACTOR_MESSAGES
        let willCloseObserver = NotificationCenter.default.addObserver(
            of: sheetWindow,
            for: .willClose
        ) { [weak self] _ in
            guard let self else {
                return
            }
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Received NSWindow.willCloseNotification. Dismissing the view controller."
                )
            self.dismissViewController()
        }

        presentation.sheet = Sheet(window: sheetWindow, willCloseObserver: willCloseObserver)
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Added sheet.willCloseObserver (\((self.presentation.sheet?.willCloseObserver).map { String($0.hashValue) } ?? "nil", privacy: .public))"
            )
        #else
        // AppKit posts NSWindow and NSApplication notifications on the main thread.
        let willCloseObserver = NotificationCenter.default.addObserver(
            forName: NSWindow.willCloseNotification,
            object: sheetWindow,
            queue: nil
        ) { [weak self] _ in
            MainActor.assumeIsolated {
                guard let self else {
                    return
                }
                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): Received NSWindow.willCloseNotification. Dismissing the view controller."
                    )
                self.dismissViewController()
            }
        }

        presentation.sheet = Sheet(window: sheetWindow, willCloseObserver: willCloseObserver)
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Added sheet.willCloseObserver (\((self.presentation.sheet?.willCloseObserver).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public))"
            )
        #endif

        guard let presentingWindow = page.platformWindow() else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log("\(self, privacy: .public) \(#function, privacy: .public): No presenting window. Returning early.")
            throw Self.presentationFailedError
        }

        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Calling beginSheet on \(String(describing: ObjectIdentifier(presentingWindow)), privacy: .public) for sheet \((self.presentation.sheet?.window).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)."
            )
        presentingWindow.beginSheet(sheetWindow, completionHandler: nil)
        #else
        guard let presentingViewController = WebKit.SOAuthorizationSupport.presentingViewController(page) else {
            throw Self.presentationFailedError
        }

        presentingViewController.present(viewController, animated: true, completion: nil)
        #endif
    }

    private static var presentationFailedError: NSError {
        NSError(domain: PAL.get_AppSSO_SOErrorDomainSingleton(), code: Int(kSOErrorAuthorizationPresentationFailed))
    }

    #if WTF_PLATFORM_MAC
    private func dismissModalSheetIfNecessary() {
        guard let sheet = presentation.sheet else {
            return
        }

        if let presentingWindow = sheet.window.sheetParent {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Calling endSheet on \(String(describing: ObjectIdentifier(presentingWindow)), privacy: .public) for sheet \(String(describing: ObjectIdentifier(sheet.window)), privacy: .public)."
                )
            presentingWindow.endSheet(sheet.window)
        }

        self.presentation.sheet = nil
        #if HAVE_APPKIT_MAIN_ACTOR_MESSAGES
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Removing sheet.willCloseObserver (\(sheet.willCloseObserver.hashValue, privacy: .public))"
            )
        #else
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Removing sheet.willCloseObserver (\(String(describing: ObjectIdentifier(sheet.willCloseObserver)), privacy: .public))"
            )
        #endif
        NotificationCenter.default.removeObserver(sheet.willCloseObserver)
    }
    #endif

    func dismissViewController(deferringWhileHidden: Bool = true) {
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): viewController=\((self.presentation.viewController as AnyObject?).map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
            )

        guard presentation.viewController != nil else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log("\(self, privacy: .public) \(#function, privacy: .public): No view controller, so returning early.")
            return
        }

        #if WTF_PLATFORM_MAC
        guard presentation.sheet != nil else {
            WebKit.LogChannel.AppSSO().releaseLog
                .log("\(self, privacy: .public) \(#function, privacy: .public): No view controller or sheet window, so returning early.")
            return
        }

        // This is a workaround for an AppKit issue: <rdar://problem/59125329>.
        // sheet.window.sheetParent is nil if the parent is minimized or the host app is hidden.
        if deferringWhileHidden, let presentingWindow = page.get()?.platformWindow(), presentingWindow.isMiniaturized {
            WebKit.LogChannel.AppSSO().releaseLog
                .log(
                    "\(self, privacy: .public) \(#function, privacy: .public): Page's window is miniaturized. Waiting to dismiss until active."
                )

            #if HAVE_APPKIT_MAIN_ACTOR_MESSAGES
            guard presentation.presentingWindowDidDeminiaturizeObserver == nil else {
                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): [Miniaturized] Already has a deminiaturized observer (\(self.presentation.presentingWindowDidDeminiaturizeObserver.map { String($0.hashValue) } ?? "nil", privacy: .public)). Hidden observer is \(self.presentation.applicationDidUnhideObserver.map { String($0.hashValue) } ?? "nil", privacy: .public)"
                    )
                return
            }

            // The observer keeps the session alive until the dismissal completes.
            presentation.presentingWindowDidDeminiaturizeObserver = NotificationCenter.default.addObserver(
                of: presentingWindow,
                for: .didDeminiaturize
            ) { _ in
                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): Window has deminiaturized. Completing the dismissal."
                    )
                self.dismissViewController()
                if let observer = self.presentation.presentingWindowDidDeminiaturizeObserver.take() {
                    NotificationCenter.default.removeObserver(observer)
                }
            }
            #else
            guard presentation.presentingWindowDidDeminiaturizeObserver == nil else {
                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): [Miniaturized] Already has a deminiaturized observer (\(self.presentation.presentingWindowDidDeminiaturizeObserver.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)). Hidden observer is \(self.presentation.applicationDidUnhideObserver.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
                    )
                return
            }

            // The observer keeps the session alive until the dismissal completes.
            presentation.presentingWindowDidDeminiaturizeObserver = NotificationCenter.default.addObserver(
                forName: NSWindow.didDeminiaturizeNotification,
                object: presentingWindow,
                queue: nil
            ) { _ in
                MainActor.assumeIsolated {
                    WebKit.LogChannel.AppSSO().releaseLog
                        .log(
                            "\(self, privacy: .public) \(#function, privacy: .public): Window has deminiaturized. Completing the dismissal."
                        )
                    self.dismissViewController()
                    if let observer = self.presentation.presentingWindowDidDeminiaturizeObserver.take() {
                        NotificationCenter.default.removeObserver(observer)
                    }
                }
            }
            #endif
            return
        }

        if deferringWhileHidden && NSApp.isHidden {
            WebKit.LogChannel.AppSSO().releaseLog
                .log("\(self, privacy: .public) \(#function, privacy: .public): Application is hidden. Waiting to dismiss until active.")

            #if HAVE_APPKIT_MAIN_ACTOR_MESSAGES
            guard presentation.applicationDidUnhideObserver == nil else {
                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): [Hidden] Already has an Unhide observer (\(self.presentation.presentingWindowDidDeminiaturizeObserver.map { String($0.hashValue) } ?? "nil", privacy: .public)). Deminiaturized observer is \(self.presentation.applicationDidUnhideObserver.map { String($0.hashValue) } ?? "nil", privacy: .public)"
                    )
                return
            }

            // The observer keeps the session alive until the dismissal completes.
            presentation.applicationDidUnhideObserver = NotificationCenter.default.addObserver(
                of: NSApp,
                for: .didUnhide
            ) { _ in
                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): Application is no longer hidden. Completing the dismissal."
                    )
                self.dismissViewController()
                if let observer = self.presentation.applicationDidUnhideObserver.take() {
                    NotificationCenter.default.removeObserver(observer)
                }
            }
            #else
            guard presentation.applicationDidUnhideObserver == nil else {
                WebKit.LogChannel.AppSSO().releaseLog
                    .log(
                        "\(self, privacy: .public) \(#function, privacy: .public): [Hidden] Already has an Unhide observer (\(self.presentation.presentingWindowDidDeminiaturizeObserver.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)). Deminiaturized observer is \(self.presentation.applicationDidUnhideObserver.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)"
                    )
                return
            }

            // The observer keeps the session alive until the dismissal completes.
            presentation.applicationDidUnhideObserver = NotificationCenter.default.addObserver(
                forName: NSApplication.didUnhideNotification,
                object: NSApp,
                queue: nil
            ) { _ in
                MainActor.assumeIsolated {
                    WebKit.LogChannel.AppSSO().releaseLog
                        .log(
                            "\(self, privacy: .public) \(#function, privacy: .public): Application is no longer hidden. Completing the dismissal."
                        )
                    self.dismissViewController()
                    if let observer = self.presentation.applicationDidUnhideObserver.take() {
                        NotificationCenter.default.removeObserver(observer)
                    }
                }
            }
            #endif
            return
        }

        dismissModalSheetIfNecessary()
        #if HAVE_APPKIT_MAIN_ACTOR_MESSAGES
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Finished call with deminiaturized observer (\(self.presentation.presentingWindowDidDeminiaturizeObserver.map { String($0.hashValue) } ?? "nil", privacy: .public)) and Hidden observer (\(self.presentation.applicationDidUnhideObserver.map { String($0.hashValue) } ?? "nil", privacy: .public))"
            )
        #else
        WebKit.LogChannel.AppSSO().releaseLog
            .log(
                "\(self, privacy: .public) \(#function, privacy: .public): Finished call with deminiaturized observer (\(self.presentation.presentingWindowDidDeminiaturizeObserver.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public)) and Hidden observer (\(self.presentation.applicationDidUnhideObserver.map { String(describing: ObjectIdentifier($0)) } ?? "nil", privacy: .public))"
            )
        #endif
        #else
        presentation.viewController?.presentingViewController?.dismiss(animated: true, completion: nil)
        #endif

        presentation.viewController = nil
    }
}

#endif
