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

#import "Device.h"

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JavaScriptCore.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * A simple package of data that is manipulated by the tasks.  The exact layout
 * of the payload data carried by a packet is not importaint, and neither is the
 * nature of the work performed on packets by the tasks.
 *
 * Besides carrying data, packets form linked lists and are hence used both as
 * data and worklists.
 */
@protocol PacketExports <JSExport>

@property (nonatomic, assign) Device device;

@end

@interface Packet : NSObject <PacketExports>

@property (nonatomic, assign) Device device;
@property (nonatomic, strong, nullable) Packet *link;

+ (instancetype)create:(Device)device withLink:(nullable Packet *)link;
- (nonnull Packet *)addToQueue:(nullable Packet *)queue;

@end

@interface DevicePacket : Packet

@property (nonatomic, assign) int letterIndex;

+ (instancetype)create:(Device)device withLink:(nullable Packet *)link;

@end

@protocol WorkPacketExports <PacketExports, JSExport>

@property (nonatomic, assign) int processed;
@property (nonatomic, strong, readonly) NSArray *data;

@end

@interface WorkPacket : Packet <WorkPacketExports>

@property (nonatomic, assign) int processed;
@property (nonatomic, strong, readonly) NSArray *data;

+ (instancetype)createWithLink:(nullable Packet *)link;
+ (unsigned)size;

@end

NS_ASSUME_NONNULL_END
