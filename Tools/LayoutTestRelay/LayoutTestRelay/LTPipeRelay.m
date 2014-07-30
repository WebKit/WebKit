/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "LTPipeRelay.h"

#import <Foundation/Foundation.h>
#include <sys/stat.h>
#include <sys/types.h>

@interface LTPipeRelay ()
@property (readonly, strong) NSString *fifoPrefix;
@property (strong) NSOutputStream *standardInputPipe;
@property (strong) NSInputStream *standardOutputPipe;
@property (strong) NSInputStream *standardErrorPipe;

- (void)createFIFOs;
- (void)destroyFIFOs;
@end

@implementation LTPipeRelay

- (NSString *)inPipePath
{
    return [_fifoPrefix stringByAppendingString:@"_IN"];
}

- (NSString *)outPipePath
{
    return [_fifoPrefix stringByAppendingString:@"_OUT"];
}

- (NSString *)errorPipePath
{
    return [_fifoPrefix stringByAppendingString:@"_ERROR"];
}

- (NSOutputStream *) outputStream
{
    return _standardInputPipe;
}

- (id)initWithPrefix:(NSString *)prefix
{
    if ((self = [super init])) {
        _fifoPrefix = prefix;
        [self destroyFIFOs];
    }
    return self;
}

- (void)setup
{
    [self destroyFIFOs];
    [self createFIFOs];
}

- (void)tearDown
{
    [self destroyFIFOs];
}

-(void) connect
{
    _standardInputPipe = [NSOutputStream outputStreamToFileAtPath:[self inPipePath] append:YES];
    _standardOutputPipe = [NSInputStream inputStreamWithFileAtPath:[self outPipePath]];
    _standardErrorPipe = [NSInputStream inputStreamWithFileAtPath:[self errorPipePath]];

    [[self standardInputPipe] setDelegate:self];
    [[self standardOutputPipe] setDelegate:self];
    [[self standardErrorPipe] setDelegate:self];

    [[self standardOutputPipe] scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];
    [[self standardErrorPipe] scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSDefaultRunLoopMode];

    [[self standardInputPipe] open];
    [[self standardOutputPipe] open];
    [[self standardErrorPipe] open];

    [[self relayDelegate] didConnect];
}

- (void)disconnect
{
    [[self standardOutputPipe] close];
    [[self standardErrorPipe] close];
    [[self standardInputPipe] close];
    [[self relayDelegate] didDisconnect];
}

- (void)createFIFOs
{
    mkfifo([[self inPipePath] UTF8String], 0666);
    mkfifo([[self outPipePath] UTF8String], 0666);
    mkfifo([[self errorPipePath] UTF8String], 0666);
}

- (void)destroyFIFOs
{
    unlink([[self inPipePath] UTF8String]);
    unlink([[self outPipePath] UTF8String]);
    unlink([[self errorPipePath] UTF8String]);
}

- (void) relayStream:(NSInputStream *)stream
{
    uint8_t bytes[1024];
    NSInteger bytesRead = [stream read:bytes maxLength:1024];
    NSData *data = [NSData dataWithBytes:bytes length:bytesRead];

    if (stream == [self standardOutputPipe])
        [[self relayDelegate] didReceiveStdoutData:data];
    else
        [[self relayDelegate] didReceiveStderrData:data];
}

- (void)stream:(NSStream *)aStream handleEvent:(NSStreamEvent)eventCode
{
    switch (eventCode) {
    case NSStreamEventNone:
        break;
    case NSStreamEventHasBytesAvailable:
        [self relayStream:(NSInputStream *)aStream];
    case NSStreamEventHasSpaceAvailable:
        break;
    case NSStreamEventOpenCompleted:
        break;
    case NSStreamEventEndEncountered:
        [[self relayDelegate] didCrashWithMessage:nil];
        break;
    case NSStreamEventErrorOccurred:
        [[self relayDelegate] didCrashWithMessage:[[aStream streamError] description]];
        break;
    }
}

@end
