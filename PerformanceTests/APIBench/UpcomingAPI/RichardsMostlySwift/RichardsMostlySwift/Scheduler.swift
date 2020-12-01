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

import JavaScriptCore

@objc protocol SchedulerExports : JSExport {
    func waitCurrent() -> Task?
    func handle(packet: WorkPacket) -> Task?
}

/**
* A scheduler can be used to schedule a set of tasks based on their relative
* priorities.  Scheduling is done by maintaining a list of task control blocks
* which holds tasks and the data queue they are processing.
* @constructor
*/
@objc public class Scheduler : NSObject, SchedulerExports {
    private(set) var queueCount = 0
    private(set) var holdCount = 0
    private(set) var currentTask : Task? = nil
    private(set) var list : Task? = nil

    /**
    * Execute the tasks managed by this scheduler.
    */
    func schedule()
    {
        self.currentTask = self.list
        while let task = self.currentTask {
            var packet : Packet? = nil
            switch task.state {
            case .waitingWithPacket:
                packet = task.takePacket()
                fallthrough
            case .run, .runWithPacket:
                self.currentTask = task.run(packet: packet)
            default:
                self.currentTask = task.next
            }
        }
    }

    /**
    * Add the specified task to this scheduler.
    */
    func add(task: Task)
    {
        task.next = self.list
        self.list = task
    }

    /**
    * Release a task that is currently blocked and return the next block to run.
    */
    func release_(device: Device) -> Task?
    {
        guard let task : DeviceTask = find(where: { $0.device == device }) else {
            return nil
        }
        task.release_()
        if task.priority > self.currentTask!.priority {
            return task
        }
        return self.currentTask
    }

    /**
    * Block the currently executing task and return the next task control block
    * to run.  The blocked task will not be made runnable until it is explicitly
    * released, even if new work is added to it.
    */
    func holdCurrent() -> Task?
    {
        self.holdCount += 1
        self.currentTask!.hold()
        return self.currentTask!.next
    }

    /**
    * Suspend the currently executing task and return the next task control block
    * to run.  If new work is added to the suspended task it will be made runnable.
    */
    func waitCurrent() -> Task?
    {
        self.currentTask!.wait()
        return self.currentTask
    }

    /**
    * Add the specified packet to the end of the worklist used by the task
    * associated with the packet and make the task runnable if it is currently
    * suspended.
    */
    func queue(packet: DevicePacket) -> Task?
    {
        let task : DeviceTask? = self.find(where: { $0.device == packet.device })
        return queueImpl(task: task, packet: packet)
    }

    func queue(packet: WorkPacket) -> Task?
    {
        let task : WorkerTask? = self.find(where: { _ in true })
        return queueImpl(task: task, packet: packet)
    }

    func handle(packet: WorkPacket) -> Task?
    {
        let task : HandlerTask? = self.find(where: { $0.device == packet.device })
        return queueImpl(task: task, packet: packet)
    }

    func handle(packet: DevicePacket) -> Task?
    {
        let task : HandlerTask? = self.find(where: { $0.device == packet.device })
        return queueImpl(task: task, packet: packet)
    }

    private func queueImpl(task: Task?, packet: Packet) -> Task?
    {
        guard task != nil else { return nil }
        self.queueCount += 1
        packet.link = nil
        return task!.checkPriorityAdd(task: self.currentTask!, packet: packet)
    }

    private func find<T: Task>(where condition: (T) -> Bool) -> T?
    {
        var task = self.list
        while task != nil {
            if let t = task as? T {
                if condition(t) {
                    return t
                }
            }
            task = task!.next
        }
        return nil
    }
}
