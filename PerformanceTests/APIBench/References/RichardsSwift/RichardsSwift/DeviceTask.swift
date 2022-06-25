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

enum Device {
    case A
    case B
}

/**
 * A task that suspends itself after each time it has been run to simulate
 * waiting for data from an external device.
 */
class DeviceTask : Task {
    let device: Device
    var pendingPacket : DevicePacket? = nil

    static func create(scheduler: Scheduler, device: Device, priority: Priority)
    {
        let task = DeviceTask(scheduler: scheduler, device: device, priority: priority)
        scheduler.add(task: task)
    }

    override func run(packet: Packet?) -> Task?
    {
        if packet == nil {
            guard let packet = self.pendingPacket else {
                return self.scheduler.waitCurrent()
            }
            self.pendingPacket = nil
            return self.scheduler.handle(packet: packet)
        }

        self.pendingPacket = (packet as! DevicePacket)
        return self.scheduler.holdCurrent()
    }

    private init(scheduler: Scheduler, device: Device, priority: Priority)
    {
        self.device = device
        super.init(scheduler: scheduler, priority: priority)
    }
}
