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
#import "WebGLBlocklist.h"

#if PLATFORM(MAC)

#import "BlocklistUpdater.h"
#import <OpenGL/OpenGL.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>

namespace WebCore {

struct OSBuildInfo {
    OSBuildInfo()
        : major(0)
        , minor(0)
        , build(0)
    {
    }

    OSBuildInfo(int major, int minor, int build)
        : major(major)
        , minor(minor)
        , build(build)
    {
    }

    bool operator==(const OSBuildInfo& other) const
    {
        return major == other.major && minor == other.minor && build == other.build;
    }

    bool operator>(const OSBuildInfo& other) const
    {
        return std::tie(major, minor, build) > std::tie(other.major, other.minor, other.build);
    }

    bool operator<=(const OSBuildInfo& other) const
    {
        return std::tie(major, minor, build) <= std::tie(other.major, other.minor, other.build);
    }

    bool operator<(const OSBuildInfo& other) const
    {
        return std::tie(major, minor, build) < std::tie(other.major, other.minor, other.build);
    }

    int major; // Represents the 13 in 13C64.
    int minor; // Represents the C in 13C64 (as a number where A == 1, i.e. 3).
    int build; // Represents the 64 in 13C64.
};

static OSBuildInfo buildInfoFromOSBuildString(NSString *buildString)
{
    NSError *error = NULL;
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"^(\\d+)([A-Z])(\\d+)" options:0 error:&error];
    NSArray *matches = [regex matchesInString:buildString options:0 range:NSMakeRange(0, [buildString length])];
    if (!matches || matches.count != 1) {
#ifndef NDEBUG
        NSLog(@"WebGLBlocklist could not parse OSBuild entry: %@", buildString);
#endif
        return OSBuildInfo();
    }

    NSTextCheckingResult *matchResult = [matches objectAtIndex:0];

    if (matchResult.numberOfRanges != 4) {
#ifndef NDEBUG
        NSLog(@"WebGLBlocklist could not parse OSBuild entry: %@", buildString);
#endif
        return OSBuildInfo();
    }

    int majorVersion = [[buildString substringWithRange:[matchResult rangeAtIndex:1]] intValue];
    int minorVersion = [[buildString substringWithRange:[matchResult rangeAtIndex:2]] characterAtIndex:0] - 'A' + 1;
    int buildVersion = [[buildString substringWithRange:[matchResult rangeAtIndex:3]] intValue];

    return OSBuildInfo(majorVersion, minorVersion, buildVersion);
}

bool WebGLBlocklist::shouldBlockWebGL()
{
    BlocklistUpdater::initializeQueue();

    __block bool shouldBlock = false;
    dispatch_sync(BlocklistUpdater::queue(), ^{
        BlocklistUpdater::reloadIfNecessary();

        WebGLBlocklist* webGLBlocklist = BlocklistUpdater::webGLBlocklist();
        if (webGLBlocklist)
            shouldBlock = webGLBlocklist->shouldBlock();
    });

    return shouldBlock;
}

bool WebGLBlocklist::shouldSuggestBlockingWebGL()
{
    BlocklistUpdater::initializeQueue();

    __block bool shouldSuggestBlocking = false;
    dispatch_sync(BlocklistUpdater::queue(), ^{
        BlocklistUpdater::reloadIfNecessary();

        WebGLBlocklist* webGLBlocklist = BlocklistUpdater::webGLBlocklist();
        if (webGLBlocklist)
            shouldSuggestBlocking = webGLBlocklist->shouldSuggestBlocking();
    });

    return shouldSuggestBlocking;
}

static bool matchesGPU(GLint machineId, GLint rendererMask)
{
    // If the last two bytes of the rendererMask are not zero, then we're
    // looking for an exact GPU match. Otherwise we're matching against
    // a class of GPUs.

    if (rendererMask & 0xFF)
        return machineId == rendererMask;

    return (machineId & kCGLRendererIDMatchingMask) == rendererMask;
}

static GLint gpuMaskFromString(NSString *input)
{
    NSScanner* scanner = [NSScanner scannerWithString:input];
    unsigned maskValue;
    [scanner scanHexInt:&maskValue];
    return static_cast<GLint>(maskValue & (kCGLRendererIDMatchingMask | 0xFF));
}

static bool matchesBuildInfo(OSBuildInfo machineInfo, OSBuildInfo blockInfo, WebGLBlocklist::BlockComparison comparison)
{
    switch (comparison) {
    case WebGLBlocklist::BlockComparison::Equals:
        return machineInfo == blockInfo;
    case WebGLBlocklist::BlockComparison::LessThan:
        return machineInfo < blockInfo;
    case WebGLBlocklist::BlockComparison::LessThanEquals:
        return machineInfo <= blockInfo;
    }
}

std::unique_ptr<WebGLBlocklist> WebGLBlocklist::create(NSDictionary *propertyList)
{
    CFDictionaryRef systemVersionDictionary = _CFCopySystemVersionDictionary();
    CFStringRef osBuild = static_cast<CFStringRef>(CFDictionaryGetValue(systemVersionDictionary, _kCFSystemVersionBuildVersionKey));
    OSBuildInfo buildInfo = buildInfoFromOSBuildString((__bridge NSString *)osBuild);
    CFRelease(systemVersionDictionary);

    if (!buildInfo.major)
        return nullptr;

    NSArray *blockEntries = [propertyList objectForKey:@"WebGLBlocklist"];

    if (![blockEntries isKindOfClass:[NSArray class]] || !blockEntries.count)
        return nullptr;

    CGLPixelFormatAttribute attribs[12] = {
        kCGLPFAColorSize, (CGLPixelFormatAttribute)32,
        kCGLPFADepthSize, (CGLPixelFormatAttribute)32,
        kCGLPFAAccelerated,
        kCGLPFASupersample,
        kCGLPFAMultisample,
        kCGLPFASampleBuffers, (CGLPixelFormatAttribute)1,
        kCGLPFASamples, (CGLPixelFormatAttribute)4,
        (CGLPixelFormatAttribute)0
    };

    CGLPixelFormatObj pix;
    GLint npix;
    CGLChoosePixelFormat(attribs, &pix, &npix);
    CGLContextObj ctx;
    CGLCreateContext(pix, 0, &ctx);
    GLint rendererId = 0;
    CGLGetParameter(ctx, kCGLCPCurrentRendererID, &rendererId);
    GLint supportsSeparateAddressSpace = 0;
    CGLGetParameter(ctx, kCGLCPSupportSeparateAddressSpace, &supportsSeparateAddressSpace);
    CGLDestroyContext(ctx);
    CGLReleasePixelFormat(pix);

    rendererId &= kCGLRendererIDMatchingMask | 0xFF;

    BlockCommand globalCommand = BlockCommand::Allow;

    for (NSDictionary *blockData in blockEntries) {

        GLint gpuMask = gpuMaskFromString([blockData objectForKey:@"GPU"]);

        OSBuildInfo blockedBuildInfo = buildInfoFromOSBuildString(static_cast<NSString*>([blockData objectForKey:@"OSBuild"]));

        NSString *comparisonString = [blockData objectForKey:@"Comparison"];
        BlockComparison comparison = BlockComparison::Equals;
        if ([comparisonString isEqualToString:@"LessThan"])
            comparison = BlockComparison::LessThan;
        else if ([comparisonString isEqualToString:@"LessThanEquals"])
            comparison = BlockComparison::LessThanEquals;

        NSString *commandString = [blockData objectForKey:@"Command"];
        BlockCommand command = BlockCommand::Allow;
        if ([commandString isEqualToString:@"Block"])
            command = BlockCommand::Block;
        else if ([commandString isEqualToString:@"SuggestBlocking"])
            command = BlockCommand::SuggestBlocking;

        if (matchesGPU(rendererId, gpuMask) && matchesBuildInfo(buildInfo, blockedBuildInfo, comparison)) {
            globalCommand = command;
            break;
        }
    }

    if (!supportsSeparateAddressSpace && globalCommand == BlockCommand::Allow)
        globalCommand = BlockCommand::SuggestBlocking;

    return std::unique_ptr<WebGLBlocklist>(new WebGLBlocklist(globalCommand));
}

bool WebGLBlocklist::shouldBlock() const
{
    return m_command == BlockCommand::Block;
}

bool WebGLBlocklist::shouldSuggestBlocking() const
{
    return m_command == BlockCommand::SuggestBlocking;
}

WebGLBlocklist::WebGLBlocklist(BlockCommand command)
    : m_command(command)
{
}

WebGLBlocklist::~WebGLBlocklist()
{
}

}
#endif
