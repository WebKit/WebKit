// Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if HAVE_MOUSE_DEVICE_OBSERVATION

@_weakLinked internal import GameController
internal import WebKit_Internal

@objc @implementation extension WKMouseDeviceObserver {
    private static let shared = WKMouseDeviceObserver()

    @nonobjc private var connectionObservationTask: Task<Void, Never>? = nil
    @nonobjc private var disconnectionObservationTask: Task<Void, Never>? = nil

    private var startCount = 0
    private var connectedDeviceCount = 0 {
        didSet {
            precondition(connectedDeviceCount >= 0)

            if oldValue == 0 && connectedDeviceCount > 0 {
                mousePointerDevicesDidChange(true)
                return
            }

            if oldValue > 0 && connectedDeviceCount == 0 {
                mousePointerDevicesDidChange(false)
            }
        }
    }

    var hasMouseDevice: Bool {
        connectedDeviceCount > 0
    }

    override init() {
    }

    class func sharedInstance() -> WKMouseDeviceObserver {
        shared
    }

    func start() {
        guard #_hasSymbol(GCMouse.self) else {
            return
        }

        precondition(startCount >= 0)

        startCount += 1
        guard startCount == 1 else {
            return
        }

        connectionObservationTask = Task {
            let connectSequence = NotificationCenter.default.notifications(named: .GCMouseDidConnect).map { $0.object is GCMouse }
            for await _ in connectSequence {
                connectedDeviceCount = GCMouse.mice().count
            }
        }

        disconnectionObservationTask = Task {
            let disconnectSequence = NotificationCenter.default.notifications(named: .GCMouseDidDisconnect).map { $0.object is GCMouse }
            for await _ in disconnectSequence {
                connectedDeviceCount = GCMouse.mice().count
            }
        }
    }

    func stop() {
        guard startCount > 0 else {
            return
        }

        startCount -= 1

        guard startCount == 0 else {
            return
        }

        connectionObservationTask?.cancel()
        disconnectionObservationTask?.cancel()
    }

    func _setHasMouseDevice(forTesting hasMouseDevice: Bool) {
        connectedDeviceCount = hasMouseDevice ? 1 : 0
    }
}

#endif // HAVE_MOUSE_DEVICE_OBSERVATION
