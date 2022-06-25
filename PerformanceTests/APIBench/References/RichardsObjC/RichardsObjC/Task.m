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

#import "Task.h"

@implementation Task

+ (instancetype)createWithScheduler:(Scheduler *)scheduler priority:(Priority)priority queue:(nullable Packet *)queue
{
    Task *task = [[super alloc] init];
    task->_scheduler = scheduler;
    task->_priority = priority;
    task->_queue = queue;
    if (queue)
        task->_state |= HasPacket;
    return task;
}


- (nullable Task *)run:(nullable Packet *)packet
{
    NSLog(@"This method must be overridden");
    exit(1);
}

- (void)release_
{
    _state &= ~Held;
}

- (void)hold
{
    _state |= Held;
}

- (void)wait
{
    _state |= Waiting;
}

- (void)resume
{
    _state &= ~Waiting;
}

- (Packet *)takePacket
{
    Packet *packet = self.queue;
    _queue = packet.link;
    _state = self.queue ? RunWithPacket : Run;
    return packet;
}

- (Task *)checkPriorityAdd:(Task *)task packet:(Packet *)packet
{
    if (!self.queue) {
        _queue = packet;
        _state |= HasPacket;
        if (self.priority > task.priority)
            return self;
    } else
        _queue = [packet addToQueue:self.queue];
    return task;
}

@end
