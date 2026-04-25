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

#import "Packet.h"

@implementation Packet

+ (instancetype)create:(Device)device withLink:(Packet *)link
{
    Packet *packet = [[self alloc] init];
    packet.device = device;
    packet.link = link;
    return packet;
}

- (Packet *)addToQueue:(Packet *)queue
{
    self.link = nil;
    if (!queue)
        return self;
    Packet* next = queue;
    while (next.link)
        next = next.link;
    next.link = self;
    return queue;
}

@end

@implementation DevicePacket

+ (instancetype)create:(Device)device withLink:(Packet *)link
{
    DevicePacket *packet = [super create:device withLink:link];
    packet.letterIndex = 0;
    return packet;
}

@end

@implementation WorkPacket

+ (instancetype)createWithLink:(Packet *)link
{
    WorkPacket *packet = [super create:A withLink:link];
    packet.processed = 0;
    packet->_data = @[@0, @0, @0, @0];
    return packet;
}

+ (unsigned)size
{
    return 4;
}

@end
