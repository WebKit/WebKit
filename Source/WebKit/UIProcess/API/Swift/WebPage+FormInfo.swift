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

import Foundation

extension WebPage {
    /// A type that contains information about a form submission from a webpage.
    @MainActor
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct FormInfo {
        init(_ wrapped: WKFormInfo) {
            self.wrapped = wrapped
        }

        /// The frame where the form is being submitted will cause a navigation.
        public var targetFrame: WebPage.FrameInfo { WebPage.FrameInfo(wrapped.targetFrame) }

        /// The frame that caused the form submission.
        public var sourceFrame: WebPage.FrameInfo { WebPage.FrameInfo(wrapped.sourceFrame) }

        /// The URL that the frame is being navigated to.
        public var submissionURL: Foundation.URL { wrapped.submissionURL }

        /// The HTTP method used to submit the form; generally either @"GET" or @"POST".
        public var httpMethod: Swift.String { wrapped.httpMethod }

        /// A dictionary of the form values that will be submitted during the navigation.
        public var formValues: [Swift.String: Swift.String] { wrapped.formValues }

        var wrapped: WKFormInfo
    }
}

#endif
