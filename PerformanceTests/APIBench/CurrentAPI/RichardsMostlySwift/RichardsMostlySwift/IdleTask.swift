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
 * An idle task doesn't do any work itself but cycles control between the two device tasks.
 */
class IdleTask : Task {
    var hash_ : Int = 1
    var count : Int = 1000

    static func create(scheduler: Scheduler, priority: Priority)
    {
        let task = IdleTask(scheduler: scheduler, priority: priority)
        scheduler.add(task: task)
    }

    override func run(packet: Packet?) -> Task?
    {
      self.count -= 1
      if self.count == 0 {
          return self.scheduler.holdCurrent()
      }
      if (self.hash_ & 1) == 0 {
        self.hash_ = self.hash_ >> 1
        return self.scheduler.release_(device: .A)
      } else {
        self.hash_ = (self.hash_ >> 1) ^ 0xD008
        return self.scheduler.release_(device: .B)
      }
    }

    private init(scheduler: Scheduler, priority: Priority)
    {
      super.init(scheduler: scheduler, priority: priority)
      self.resume()
    }
}
