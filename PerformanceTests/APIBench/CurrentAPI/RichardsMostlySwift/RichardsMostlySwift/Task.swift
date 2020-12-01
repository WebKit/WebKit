/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
*/

import Foundation
import JavaScriptCore

enum Priority : Int {
    case Idle = 0
    case Worker = 1000
    case HandlerA = 2000
    case HandlerB = 3000
    case DeviceA = 4000
    case DeviceB = 5000

    static func >(lhs:Self, rhs:Self) -> Bool 
    {
        return lhs.rawValue > rhs.rawValue
    }
}

@objc public class Task : NSObject, JSExport {
    let scheduler : Scheduler
    let priority : Priority
    var next : Task? = nil
    private(set) var queue : Packet?
    private(set) var state : State = .waiting

    struct State : OptionSet {
        let rawValue: Int

        static let run : State = []
        static let hasPacket = State(rawValue: 1 << 0)
        static let waiting = State(rawValue: 1 << 1)
        static let held = State(rawValue: 1 << 2)

        static let runWithPacket = hasPacket.union(run)
        static let waitingWithPacket = hasPacket.union(waiting)

        init(rawValue: Int)
        {
            self.rawValue = rawValue
        }
    }

    func run(packet: Packet?) -> Task?
    {
        preconditionFailure("This method must be overridden") 
    }

    func release_()
    {
        self.state.remove(.held)
    }

    func hold()
    {
        self.state.insert(.held)
    }

    func wait()
    {
        self.state.insert(.waiting)
    }

    func resume()
    {
        self.state.remove(.waiting)
    }

    func takePacket() -> Packet
    {
        let packet = self.queue!
        self.queue = packet.link
        if self.queue == nil {
            self.state = .run
        } else {
            self.state = .runWithPacket
        }
        return packet
    }

    func checkPriorityAdd(task: Task, packet: Packet) -> Task
    {
        if self.queue == nil {
            self.queue = packet
            self.state.insert(.hasPacket)
            if self.priority > task.priority {
                return self
            }
        } else {
            self.queue = packet.addTo(queue: self.queue)
        }
        return task
    }

    init(scheduler: Scheduler, priority: Priority, queue: Packet? = nil)
    {
        self.scheduler = scheduler
        self.priority = priority
        self.queue = queue
        if queue != nil {
            self.state.insert(.hasPacket)
        }
    }
}
