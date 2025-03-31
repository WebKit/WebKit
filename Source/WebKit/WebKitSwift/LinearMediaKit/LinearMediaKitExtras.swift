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

#if os(visionOS)

import UIKit

#if canImport(AVKit, _version: 1270)
@_spi(LinearMediaKit) @_spi(LinearMediaKit_WebKitOnly) import AVKit
#else
@_spi(WebKitOnly) import LinearMediaKit
#endif

@objc extension PlayableViewController {
    var wks_automaticallyDockOnFullScreenPresentation: Bool {
        get {
            self.automaticallyDockOnFullScreenPresentation
        }
        set {
            self.automaticallyDockOnFullScreenPresentation = newValue
        }
    }

    var wks_dismissFullScreenOnExitingDocking: Bool {
        get {
            self.dismissFullScreenOnExitingDocking
        }
        set {
            self.dismissFullScreenOnExitingDocking = newValue
        }
    }

    var wks_environmentPickerButtonViewController: UIViewController? {
        self.environmentPickerButtonViewController
    }
}

#endif // os(visionOS)
