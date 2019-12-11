/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#import "config.h"
#import "BlacklistUpdater.h"

#if PLATFORM(MAC)

#import "PluginBlacklist.h"
#import "WebGLBlacklist.h"
#import <sys/stat.h>
#import <sys/time.h>
#import <wtf/RetainPtr.h>

// The time after which we'll check the blacklist data.
static time_t blacklistNextCheckTime;

// The number of seconds before we'll check if the blacklist data has changed.
const time_t blacklistCheckTimeInterval = 60 * 10;

// The time when we last re-parsed the blacklist file.
static time_t blacklistUpdateTime;

#if HAVE(ALTERNATE_SYSTEM_LAYOUT)
NSString * const blacklistPath = @"/Library/Apple/System/Library/CoreServices/XProtect.bundle/Contents/Resources/XProtect.meta.plist";
#else
NSString * const blacklistPath = @"/System/Library/CoreServices/XProtect.bundle/Contents/Resources/XProtect.meta.plist";
#endif

namespace WebCore {

dispatch_queue_t BlacklistUpdater::s_queue;

PluginBlacklist* BlacklistUpdater::s_pluginBlacklist = nullptr;
WebGLBlacklist* BlacklistUpdater::s_webGLBlacklist = nullptr;

NSDictionary * BlacklistUpdater::readBlacklistData()
{
    NSData *data = [NSData dataWithContentsOfFile:blacklistPath];
    if (!data)
        return nil;

    return dynamic_objc_cast<NSDictionary>([NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nullptr error:nullptr]);
}

void BlacklistUpdater::reloadIfNecessary()
{
    struct timeval timeVal;
    if (!gettimeofday(&timeVal, NULL)) {
        if (timeVal.tv_sec < blacklistNextCheckTime)
            return;
    }

    blacklistNextCheckTime = timeVal.tv_sec + blacklistCheckTimeInterval;

    struct stat statBuf;
    if (stat([blacklistPath fileSystemRepresentation], &statBuf) == -1)
        return;

    if (statBuf.st_mtimespec.tv_sec == blacklistUpdateTime)
        return;
    NSDictionary *propertyList = readBlacklistData();
    if (!propertyList)
        return;

    if (s_pluginBlacklist) {
        delete s_pluginBlacklist;
        s_pluginBlacklist = 0;
    }

    if (s_webGLBlacklist) {
        delete s_webGLBlacklist;
        s_webGLBlacklist = 0;
    }

    s_pluginBlacklist = PluginBlacklist::create(propertyList).release();
    s_webGLBlacklist = WebGLBlacklist::create(propertyList).release();

    blacklistUpdateTime = statBuf.st_mtimespec.tv_sec;
}

void BlacklistUpdater::initializeQueue()
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        s_queue = dispatch_queue_create("com.apple.WebKit.Blacklist", 0);
    });
}

}

#endif
