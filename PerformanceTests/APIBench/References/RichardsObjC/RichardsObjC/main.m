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

#import "DeviceTask.h"
#import "HandlerTask.h"
#import "IdleTask.h"
#import "Packet.h"
#import "Scheduler.h"
#import "WorkerTask.h"

#import <QuartzCore/QuartzCore.h>

static const unsigned EXPECTED_QUEUE_COUNT = 2322;
static const unsigned EXPECTED_HOLD_COUNT = 928;

void runRichards()
{
    Scheduler *scheduler = [[Scheduler alloc] init];

    [IdleTask createWithScheduler:scheduler priority:Idle];

    {
        WorkPacket *queue = [WorkPacket createWithLink:nil];
        queue = [WorkPacket createWithLink:queue];
        [WorkerTask createWithScheduler:scheduler priority:Worker queue:queue];
    }

    {
        DevicePacket *queue = [DevicePacket create:A withLink:nil];
        queue = [DevicePacket create:A withLink: queue];
        queue = [DevicePacket create:A withLink: queue];
        [HandlerTask createWithScheduler:scheduler device:A priority:HandlerA queue:queue];
    }

    {
        DevicePacket *queue = [DevicePacket create:B withLink:nil];
        queue = [DevicePacket create:B withLink:queue];
        queue = [DevicePacket create:B withLink:queue];
        [HandlerTask createWithScheduler:scheduler device:B priority:HandlerB queue:queue];
    }

    [DeviceTask createWithScheduler:scheduler device:A priority:DeviceA];
    [DeviceTask createWithScheduler:scheduler device:B priority:DeviceB];

    [scheduler schedule];

    if (scheduler.queueCount != EXPECTED_QUEUE_COUNT || scheduler.holdCount != EXPECTED_HOLD_COUNT) {
        NSLog(@"Error during execution: queueCount = %d, holdCount = %d.", scheduler.queueCount, scheduler.holdCount);
        exit(1);
    }
}


int main()
{
    const unsigned iterations = 50;
    const CFTimeInterval start = CACurrentMediaTime();
    for (unsigned i = 0; i < iterations; ++i)
        runRichards();
    const CFTimeInterval end = CACurrentMediaTime();
    printf("%d", (int)((end-start) * 1000));
    return 0;
}
