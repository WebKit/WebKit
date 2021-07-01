// Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if canImport(GroupActivities)

import AVFoundation
import Combine
@_spi(Safari) import GroupActivities
import Foundation

@objc public enum WKGroupSessionState : Int {
    case waiting = 0
    case joined = 1
    case invalidated = 2
}

@objc(WKURLActivity)
public final class WKURLActivityWrapper : NSObject {
    private var urlActivity: URLActivity
    init(activity: URLActivity) {
        self.urlActivity = activity
        super.init()
    }

    @objc public var fallbackURL: URL? {
        return self.urlActivity.webpageURL
    }
}

@objc(WKGroupSession)
public final class WKGroupSessionWrapper : NSObject {
    private var groupSession: GroupSession<URLActivity>
    private var activityWrapper : WKURLActivityWrapper
    private var cancellables: Set<AnyCancellable> = []

    init(groupSession: GroupSession<URLActivity>) {
        self.groupSession = groupSession
        self.activityWrapper = WKURLActivityWrapper(activity: groupSession.activity)

        super.init()

        self.groupSession.$activity
            .sink { [unowned self] in self.activityChanged(activity: $0) }
            .store(in: &cancellables)
        self.groupSession.$state
            .sink { [unowned self] in self.stateChanged(state: $0) }
            .store(in: &cancellables)
    }

    @objc public var activity: WKURLActivityWrapper { self.activityWrapper }
    @objc public var uuid: UUID { groupSession.id }

    private static func wrapperSessionState(state: GroupSession<URLActivity>.State) -> WKGroupSessionState {
        switch state {
        case .waiting:
            return WKGroupSessionState.waiting
        case .joined:
            return WKGroupSessionState.joined
        case .invalidated:
            return WKGroupSessionState.invalidated
        @unknown default:
            // Unanticipated state value.
            assertionFailure()
            return WKGroupSessionState.invalidated
        }
    }

    @objc public var state: WKGroupSessionState {
        return WKGroupSessionWrapper.wrapperSessionState(state: groupSession.state)
    }

    @objc public var newActivityCallback: ((WKURLActivityWrapper) -> Void)?
    @objc public var stateChangedCallback: ((WKGroupSessionState) -> Void)?

    @objc public func join() {
        groupSession.join()
    }

    @objc public func leave() {
        groupSession.leave()
    }

    @objc(coordinateWithCoordinator:)
    public func coordinate(coordinator: AVPlaybackCoordinator) {
        coordinator.coordinateWithSession(self.groupSession)
    }

    private func activityChanged(activity: URLActivity) {
        self.activityWrapper = WKURLActivityWrapper(activity: groupSession.activity)

        guard let callback = newActivityCallback else {
            return
        }

        callback(activityWrapper)
    }

    private func stateChanged(state: GroupSession<URLActivity>.State) {
        guard let callback = stateChangedCallback else {
            return
        }

        callback(WKGroupSessionWrapper.wrapperSessionState(state: state))
    }
}

@objc(WKGroupSessionObserver)
public class WKGroupSessionObserver : NSObject {
    @objc public var newSessionCallback: ((WKGroupSessionWrapper) -> Void)?

    private var incomingSessionsTask: Task.Handle<Void, Never>?

    @objc public override init() {
        super.init()

        incomingSessionsTask = detach { [weak self] in
            for await newSession in URLActivity.self.sessions() {
                DispatchQueue.main.async { [weak self] in
                    self?.receivedSession(newSession)
                }
            }
        }
    }

    deinit {
        incomingSessionsTask?.cancel()
    }

    private func receivedSession(_ session: GroupSession<URLActivity>) {
        guard let callback = newSessionCallback else {
            return
        }

        let sessionWrapper = WKGroupSessionWrapper(groupSession: session)
        callback(sessionWrapper)
    }
}

#endif
