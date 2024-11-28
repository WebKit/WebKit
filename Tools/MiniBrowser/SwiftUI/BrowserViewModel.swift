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

#if compiler(>=6.0)

import Foundation
import os
import Observation
@_spi(Private) import WebKit

@Observable
@MainActor
final class BrowserViewModel {
    typealias WebPage = WebPage_v0

    private static let logger = Logger(subsystem: Bundle.main.bundleIdentifier!, category: String(describing: BrowserViewModel.self))

    let page = WebPage()

    var displayedURL: String = ""

    func didReceiveNavigationEvent(_ event: WebPage.NavigationEvent) {
        Self.logger.info("Did receive navigation event \(String(describing: event.kind)) for navigation \(String(describing: event.navigationID))")
    }

    func navigateToSubmittedURL() {
        guard let url = URL(string: displayedURL) else {
            return
        }

        let request = URLRequest(url: url)
        self.page.load(request)
    }
}

#endif
