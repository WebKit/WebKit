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

#import "HandlerTask.h"

@implementation HandlerTask
    
+ (instancetype)createWithScheduler:(Scheduler *)scheduler device:(Device)device priority:(Priority)priority queue:(Packet *)queue
{
    HandlerTask *task = [super createWithScheduler:scheduler priority:priority queue:queue];
    task->_device = device;
    [scheduler addTask:task];
    return task;
}

- (nullable Task *)run:(nullable Packet *)packet
{
    if (packet) {
        if ([packet isKindOfClass:[WorkPacket class]])
            _workQueue = [packet addToQueue:self.workQueue];
        else
            _deviceQueue = [packet addToQueue:self.deviceQueue];
    }
    if (self.workQueue) {
        WorkPacket *workPacket = (WorkPacket *)self.workQueue;
        if (workPacket.processed < WorkPacket.size) {
            if (self.deviceQueue) {
                DevicePacket *devicePacket = (DevicePacket *)self.deviceQueue;
                _deviceQueue = devicePacket.link;
                devicePacket.letterIndex = [workPacket.data[workPacket.processed] intValue];
                ++workPacket.processed;
                return [self.scheduler queue:devicePacket];
            }
        } else {
            _workQueue = workPacket.link;
            return [self.scheduler queue:workPacket];
        }
    }
    return [self.scheduler waitCurrent];
}


@end
