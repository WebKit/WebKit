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

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JavaScriptCore.h>

NS_ASSUME_NONNULL_BEGIN

@class Task;

@protocol SchedulerExports <JSExport>

- (nullable Task *)waitCurrent;
- (nullable Task *)handle:(Packet *)packet;

@end

/**
* A scheduler can be used to schedule a set of tasks based on their relative
* priorities.  Scheduling is done by maintaining a list of task control blocks
* which holds tasks and the data queue they are processing.
*/
@interface Scheduler : NSObject <SchedulerExports>

@property (nonatomic, assign, readonly) unsigned queueCount;
@property (nonatomic, assign, readonly) unsigned holdCount;
@property (nonatomic, strong, readonly, nullable) Task *currentTask;
@property (nonatomic, strong, readonly, nullable) Task *list;

/**
* Execute the tasks managed by this scheduler.
*/
- (void)schedule;

/**
* Add the specified task to this scheduler.
*/
- (void)addTask:(Task *)task;

/**
* Release a task that is currently blocked and return the next block to run.
*/
- (nullable Task *)releaseDevice:(Device)device;

/**
* Block the currently executing task and return the next task control block
* to run.  The blocked task will not be made runnable until it is explicitly
* released, even if new work is added to it.
*/
- (nullable Task *)holdCurrent;

/**
* Suspend the currently executing task and return the next task control block
* to run.  If new work is added to the suspended task it will be made runnable.
*/
- (nullable Task *)waitCurrent;

/**
* Add the specified packet to the end of the worklist used by the task
* associated with the packet and make the task runnable if it is currently
* suspended.
*/
- (nullable Task *)queue:(Packet *)packet;

- (nullable Task *)handle:(Packet *)packet;

@end

NS_ASSUME_NONNULL_END
