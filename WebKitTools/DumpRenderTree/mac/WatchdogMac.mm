/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#import "WatchdogMac.h"

#import <Cocoa/Cocoa.h>

#import <sys/types.h>
#import <unistd.h>

void WatchdogMac::handleHang()
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    NSLog(@"Detected hang in DumpRenderTree");

    NSTask *task = [[NSTask alloc] init];
    [task setLaunchPath:@"/usr/bin/sample"];
    
    NSString *pidString = [NSString stringWithFormat:@"%i", (int)getpid()];
    [task setArguments:[NSArray arrayWithObjects:pidString, @"10", @"10", @"-file", @"/dev/stdout", nil]];
    
    [task setStandardOutput:[NSPipe pipe]];
    [task setStandardError:[task standardOutput]];
    
    [task launch];        
    NSData *outputData = [[[task standardOutput] fileHandleForReading] readDataToEndOfFile];
    [task release];
    
    NSString *sampleResult = [[NSString alloc] initWithData:outputData encoding:NSUTF8StringEncoding];
    
    NSString *reportFile = [@"~/Library/Logs/DumpRenderTree/HangReport.txt" stringByExpandingTildeInPath];
    [[NSFileManager defaultManager] createDirectoryAtPath:[@"~/Library/Logs/DumpRenderTree" stringByExpandingTildeInPath] attributes:nil];

    if ([sampleResult writeToFile:reportFile atomically:NO encoding:NSUTF8StringEncoding error:NULL])
        NSLog(@"Hang report written to %@", reportFile);
    
    [sampleResult release];
    [pool drain];
}
