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

#if ENABLE_SWIFTUI && compiler(>=6.0)

import Foundation

extension WebPage {
    /// An opaque identifier which can be used to uniquely identify a load request for a web page.
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct NavigationID: Sendable, Hashable, Equatable {
        let rawValue: ObjectIdentifier

        init(_ cocoaNavigation: WKNavigation) {
            self.rawValue = ObjectIdentifier(cocoaNavigation)
        }
    }

    /// A particular state that occurs during the progression of a navigation.
    @available(WK_IOS_TBA, WK_MAC_TBA, WK_XROS_TBA, *)
    @available(watchOS, unavailable)
    @available(tvOS, unavailable)
    public struct NavigationEvent: Sendable {
        /// A set of values representing the possible types a NavigationEvent can represent.
        public enum Kind: Sendable {
            /// This event occurs when the web page receives provisional approval to process a navigation request,
            /// but before it receives a response to that request.
            case startedProvisionalNavigation

            /// This event occurs when the web page received a server redirect for a request.
            case receivedServerRedirect

            /// This event occurs when the web page has started to receive content for the main frame.
            /// This happens immediately before the web page starts to update the main frame.
            case committed

            /// This event occurs once the navigation is complete.
            case finished

            /// This event indicates an error occurs during the early navigation process.
            case failedProvisionalNavigation(underlyingError: any Error)

            /// This event indicates an error occurred during navigation.
            case failed(underlyingError: any Error)
        }

        @_spi(Testing)
        public init(kind: Kind, navigationID: NavigationID) {
            self.kind = kind
            self.navigationID = navigationID
        }

        /// The type of this navigation event.
        public let kind: Kind

        /// The ID of the navigation that triggered this event. Multiple sequential events will have the same navigation identifier.
        public let navigationID: NavigationID
    }
}

#endif
