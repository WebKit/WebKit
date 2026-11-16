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

/**
 * A task that manipulates work packets and then suspends itself.
 */
class HandlerTask : Task {
    var device: Device
    var workQueue : WorkPacket? = nil
    var deviceQueue : DevicePacket? = nil

    static func create(scheduler: Scheduler, device: Device, priority: Priority, queue: DevicePacket)
    {
        let task = HandlerTask(scheduler: scheduler, device: device, priority: priority, queue: queue)
        scheduler.add(task: task)
    }

    override func run(packet: Packet?) -> Task?
    {
        if let p = packet {
            if let work = p as? WorkPacket {
                self.workQueue = work.addTo(queue: self.workQueue) as! WorkPacket?
            } else {
                self.deviceQueue = (p as! DevicePacket).addTo(queue: self.deviceQueue) as! DevicePacket?
            }
        }
        if let workPacket = self.workQueue {
            let processed = workPacket.processed
            if (processed < WorkPacket.SIZE) {
                if let devicePacket = self.deviceQueue {
                    self.deviceQueue = devicePacket.link as! DevicePacket?
                    devicePacket.letterIndex = workPacket.data[processed]
                    workPacket.processed = processed + 1
                    return self.scheduler.queue(packet: devicePacket)
                }
            } else {
                self.workQueue = workPacket.link as! WorkPacket?
                return self.scheduler.queue(packet: workPacket)
            }
        }
        return self.scheduler.waitCurrent()
    }

    private init(scheduler: Scheduler, device: Device, priority: Priority, queue: DevicePacket)
    {
        self.device = device
        super.init(scheduler: scheduler, priority: priority, queue: queue)
    }
}
