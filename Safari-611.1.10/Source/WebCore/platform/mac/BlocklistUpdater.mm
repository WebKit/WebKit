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
#import "BlocklistUpdater.h"

#if PLATFORM(MAC)

#import "PluginBlocklist.h"
#import "WebGLBlocklist.h"
#import <sys/stat.h>
#import <sys/time.h>
#import <wtf/RetainPtr.h>

// The time after which we'll check the blocklist data.
static time_t blocklistNextCheckTime;

// The number of seconds before we'll check if the blocklist data has changed.
const time_t blocklistCheckTimeInterval = 60 * 10;

// The time when we last re-parsed the blocklist file.
static time_t blocklistUpdateTime;

#if HAVE(READ_ONLY_SYSTEM_VOLUME)
NSString * const blocklistPath = @"/Library/Apple/System/Library/CoreServices/XProtect.bundle/Contents/Resources/XProtect.meta.plist";
#else
NSString * const blocklistPath = @"/System/Library/CoreServices/XProtect.bundle/Contents/Resources/XProtect.meta.plist";
#endif

namespace WebCore {

dispatch_queue_t BlocklistUpdater::s_queue;

PluginBlocklist* BlocklistUpdater::s_pluginBlocklist = nullptr;
WebGLBlocklist* BlocklistUpdater::s_webGLBlocklist = nullptr;

NSDictionary * BlocklistUpdater::readBlocklistData()
{
    NSData *data = [NSData dataWithContentsOfFile:blocklistPath];
    if (!data)
        return nil;

    return dynamic_objc_cast<NSDictionary>([NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nullptr error:nullptr]);
}

void BlocklistUpdater::reloadIfNecessary()
{
    struct timeval timeVal;
    if (!gettimeofday(&timeVal, NULL)) {
        if (timeVal.tv_sec < blocklistNextCheckTime)
            return;
    }

    blocklistNextCheckTime = timeVal.tv_sec + blocklistCheckTimeInterval;

    struct stat statBuf;
    if (stat([blocklistPath fileSystemRepresentation], &statBuf) == -1)
        return;

    if (statBuf.st_mtimespec.tv_sec == blocklistUpdateTime)
        return;
    NSDictionary *propertyList = readBlocklistData();
    if (!propertyList)
        return;

    if (s_pluginBlocklist) {
        delete s_pluginBlocklist;
        s_pluginBlocklist = 0;
    }

    if (s_webGLBlocklist) {
        delete s_webGLBlocklist;
        s_webGLBlocklist = 0;
    }

    s_pluginBlocklist = PluginBlocklist::create(propertyList).release();
    s_webGLBlocklist = WebGLBlocklist::create(propertyList).release();

    blocklistUpdateTime = statBuf.st_mtimespec.tv_sec;
}

void BlocklistUpdater::initializeQueue()
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        s_queue = dispatch_queue_create("com.apple.WebKit.Blocklist", 0);
    });
}

}

#endif
