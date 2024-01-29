// Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#if canImport(GroupActivities_Private)

import AVFoundation
import Combine
@_spi(Safari) import GroupActivities
import Foundation

@available(macOS 12.0, iOS 15.0, *)
@objc(WKGroupSessionState)
public enum GroupSessionState : Int {
    case waiting = 0
    case joined = 1
    case invalidated = 2
}

@available(macOS 12.0, iOS 15.0, *)
@objc(WKURLActivity)
public final class URLActivityWrapper : NSObject {
    private var urlActivity: URLActivity
    init(activity: URLActivity) {
        self.urlActivity = activity
        super.init()
    }

    @objc public var fallbackURL: URL? {
        return self.urlActivity.webpageURL
    }
}

@available(macOS 12.0, iOS 15.0, *)
@objc(WKGroupSession)
public final class GroupSessionWrapper : NSObject {
    private var groupSession: GroupSession<URLActivity>
    private var activityWrapper: URLActivityWrapper
    private var cancellables: Set<AnyCancellable> = []

    init(groupSession: GroupSession<URLActivity>) {
        self.groupSession = groupSession
        self.activityWrapper = .init(activity: groupSession.activity)

        super.init()

        self.groupSession.$activity
            .sink { [unowned self] in self.activityChanged(activity: $0) }
            .store(in: &cancellables)
        self.groupSession.$state
            .sink { [unowned self] in self.stateChanged(state: $0) }
            .store(in: &cancellables)
    }

    @objc public var activity: URLActivityWrapper { self.activityWrapper }
    @objc public var uuid: UUID { groupSession.id }

    private static func wrapperSessionState(state: GroupSession<URLActivity>.State) -> GroupSessionState {
        switch state {
        case .waiting:
            return GroupSessionState.waiting
        case .joined:
            return GroupSessionState.joined
        case .invalidated:
            return GroupSessionState.invalidated
        @unknown default:
            // Unanticipated state value.
            assertionFailure()
            return GroupSessionState.invalidated
        }
    }

    @objc public var state: GroupSessionState {
        return Self.wrapperSessionState(state: groupSession.state)
    }

    @objc public var newActivityCallback: ((URLActivityWrapper) -> Void)?
    @objc public var stateChangedCallback: ((GroupSessionState) -> Void)?

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
        self.activityWrapper = .init(activity: groupSession.activity)

        guard let callback = newActivityCallback else {
            return
        }

        callback(activityWrapper)
    }

    private func stateChanged(state: GroupSession<URLActivity>.State) {
        guard let callback = stateChangedCallback else {
            return
        }

        callback(Self.wrapperSessionState(state: state))
    }
}

@available(macOS 12.0, iOS 15.0, *)
@objc(WKGroupSessionObserver)
public class GroupSessionObserver : NSObject {
    @objc public var newSessionCallback: ((GroupSessionWrapper) -> Void)?

    private var incomingSessionsTask: Task<Void, Never>?

    @objc public override init() {
        super.init()

        incomingSessionsTask = Task.detached { [weak self] in
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

        callback(.init(groupSession: session))
    }
}

#endif // canImport(GroupActivities_Private)
