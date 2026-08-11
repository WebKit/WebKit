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

class WorkerTask : Task {
    private var device : Device = .A
    private var letterIndex : Int = 0

    static func create(scheduler: Scheduler, priority: Priority, queue: Packet)
    {
        let task = WorkerTask(scheduler: scheduler, priority: priority, queue: queue)
        scheduler.add(task: task)
    }

    override func run(packet: Packet?) -> Task?
    {
        if packet == nil {
            return self.scheduler.waitCurrent()
        }

        self.device = self.device == .A ? .B : .A

        let work = packet as! WorkPacket
        work.device = self.device
        work.processed = 0
        for i in 0...(WorkPacket.SIZE - 1) {
            self.letterIndex += 1
            if self.letterIndex > 26 {
                self.letterIndex = 1
            }
            work.data[i] = self.letterIndex
        }
        return self.scheduler.handle(packet: work)
    }

    private init(scheduler: Scheduler, priority: Priority, queue: Packet)
    {
        super.init(scheduler: scheduler, priority: priority, queue: queue)
    }
}
