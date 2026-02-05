// Copyright (C) 2024 Apple Inc. All rights reserved.
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

import Foundation

extension WebPage {
    /// A particular state that occurs during the progression of a navigation.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public enum NavigationEvent: Hashable, Sendable {
        /// This event occurs when the page receives provisional approval to process a navigation request,
        /// but before it receives a response to that request.
        case startedProvisionalNavigation

        /// This event occurs when the page received a server redirect for a request.
        case receivedServerRedirect

        /// This event occurs when the page has started to receive content for the main frame.
        ///
        /// This happens immediately before the page starts to update the main frame.
        case committed

        /// This event occurs once the navigation is complete.
        case finished
    }

    /// A specific error that caused a navigation to fail.
    @available(iOS 26.0, macOS 26.0, visionOS 26.0, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public enum NavigationError: Error {
        /// An error occurred during the early navigation process.
        case failedProvisionalNavigation(any Error)

        /// The navigation could not begin because the page has been closed.
        case pageClosed

        /// The process for the web content of this page was terminated for any reason.
        case webContentProcessTerminated

        /// The URL to navigate to is invalid.
        case invalidURL
    }
}

#endif // ENABLE_SWIFTUI
