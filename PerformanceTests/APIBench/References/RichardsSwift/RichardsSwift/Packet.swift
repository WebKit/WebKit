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
 * A simple package of data that is manipulated by the tasks.  The exact layout
 * of the payload data carried by a packet is not importaint, and neither is the
 * nature of the work performed on packets by the tasks.
 *
 * Besides carrying data, packets form linked lists and are hence used both as
 * data and worklists.
 */
class Packet {
    var device : Device
    var link : Packet?

    init(device: Device, link: Packet?)
    {
        self.device = device
        self.link = link
    }

    /**
    * Add this packet to the end of a worklist, and return the worklist.
    */
    func addTo(queue: Packet?) -> Packet? {
        self.link = nil
        guard var next = queue else {
            return self
        }
        while next.link != nil {
            next = next.link!
        }
        next.link = self
        return queue;
    }
}

final class DevicePacket : Packet {
    var letterIndex : Int = 0
}

final class WorkPacket : Packet {
    static let SIZE : Int = 4

    var processed : Int = 0
    var data : [Int] = [Int](repeating: 0, count: SIZE)

    init(link: Packet?)
    {
        super.init(device: .A, link: link)
    }
}
