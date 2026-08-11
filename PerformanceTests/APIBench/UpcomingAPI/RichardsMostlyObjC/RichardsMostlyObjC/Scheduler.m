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

#import "Scheduler.h"

#import "DeviceTask.h"
#import "HandlerTask.h"
#import "Task.h"
#import "WorkerTask.h"

@implementation Scheduler
    
- (void)schedule
{
    _currentTask = self.list;
    Task *task;
    while ((task = self.currentTask)) {
        Packet *packet = nil;
        switch (task.state) {
        case WaitingWithPacket:
            packet = [task takePacket];
        case Run:
        case RunWithPacket:
            _currentTask = [task run:packet];
            break;
        default:
            _currentTask = task.next;
            break;
        }
    }
}
    
- (void)addTask:(Task *)task
{
    task.next = self.list;
    _list = task;
}

- (nullable Task *)releaseDevice:(Device)device
{
    Task *task = [self _find:^bool(Task *task) {
        return [task isKindOfClass:[DeviceTask class]] && ((DeviceTask*)task).device == device;
    }];
    if (!task)
        return nil;
    [task release_];
    if (task.priority > self.currentTask.priority)
        return task;
    return self.currentTask;
}

- (nullable Task *)holdCurrent
{
    ++_holdCount;
    [self.currentTask hold];
    return [self.currentTask next];
}

- (nullable Task *)waitCurrent
{
    [self.currentTask wait];
    return self.currentTask;
}

- (nullable Task *)queue:(Packet *)packet
{
    bool (^predicate)(Task *);
    if ([packet isKindOfClass:[DevicePacket class]]) {
        predicate = ^bool(Task *task) {
            return [task isKindOfClass:[DeviceTask class]] && ((DeviceTask *)task).device == packet.device;
        };
    } else {
        predicate = ^bool(Task *task) {
            return [task isKindOfClass:[WorkerTask class]];
        };
    }
    Task *task = [self _find:predicate];
    return [self _queueImpl:task packet:packet];
}

- (nullable Task *)handle:(Packet *)packet
{
    Task *task = [self _find:^bool(Task *task) {
        return [task isKindOfClass:[HandlerTask class]] && ((HandlerTask*)task).device == packet.device;
    }];
    return [self _queueImpl:task packet:packet];
}


- (nullable Task *)_queueImpl:(nullable Task*)task packet:(Packet *)packet
{
    if (!task)
        return nil;
    ++_queueCount;
    packet.link = nil;
    return [task checkPriorityAdd:self.currentTask packet:packet];
}

- (nullable Task *)_find:(bool (^)(Task *))predicate
{
    Task *task = self.list;
    while (task) {
        if (predicate(task))
            return task;
        task = task.next;
    }
    return nil;
}

@end
