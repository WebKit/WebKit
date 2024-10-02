/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WebClipCache.h"

#if ENABLE(WEB_PUSH_NOTIFICATIONS) && PLATFORM(IOS)

#import "Logging.h"
#import "UIKitSPI.h"
#import <Foundation/Foundation.h>
#import <wtf/FileSystem.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/Vector.h>

namespace WebPushD {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebClipCache);

WebClipCache::WebClipCache(const String& path)
    : m_path(path)
{
    load();
}

static bool webClipExists(const String& webClipIdentifier)
{
    if (webClipIdentifier.isEmpty())
        return false;

    @autoreleasepool {
        NSString *path = [UIWebClip pathForWebClipWithIdentifier:(NSString *)webClipIdentifier];
        if (!path)
            return false;
        return [[NSFileManager defaultManager] fileExistsAtPath:path];
    }
}

static String webClipIdentifierForOrigin(const String& bundleIdentifier, const WebCore::SecurityOriginData& origin)
{
    RetainPtr<NSString> oldestMatchingIdentifier;
    WallTime oldestMatchingCreationTime = WallTime::infinity();

    @autoreleasepool {
        NSArray *webClips = [UIWebClip webClips];

        for (UIWebClip *webClip in webClips) {
            NSString *identifier = webClip.identifier;
            if (!identifier)
                continue;

            auto clipOrigin = WebCore::SecurityOriginData::fromURL(webClip.pageURL);
            if (origin != clipOrigin)
                continue;

            if (bundleIdentifier != "com.apple.SafariViewService"_s && [webClip respondsToSelector:@selector(trustedClientBundleIdentifiers)]) {
                if (![webClip.trustedClientBundleIdentifiers containsObject:(NSString *)bundleIdentifier])
                    continue;
            }

            NSString *path = [UIWebClip pathForWebClipWithIdentifier:identifier];
            if (!path)
                continue;

            auto maybeCreationTime = FileSystem::fileCreationTime(path);
            if (!maybeCreationTime)
                continue;
            auto creationTime = *maybeCreationTime;

            if (creationTime < oldestMatchingCreationTime) {
                oldestMatchingIdentifier = identifier;
                oldestMatchingCreationTime = creationTime;
            }
        }
    }

    return oldestMatchingIdentifier.get();
}

String WebClipCache::preferredWebClipIdentifier(const String& bundleIdentifier, const WebCore::SecurityOriginData& origin)
{
    bool dirty = false;

    auto it = m_preferredWebClipIdentifiers.find(std::make_tuple(bundleIdentifier, origin));
    if (it != m_preferredWebClipIdentifiers.end()) {
        auto identifier = it->value;
        if (webClipExists(identifier))
            return identifier;

        m_preferredWebClipIdentifiers.remove(it);
        dirty = true;
    }

    auto webClipIdentifier = webClipIdentifierForOrigin(bundleIdentifier, origin);
    if (!webClipIdentifier.isEmpty()) {
        m_preferredWebClipIdentifiers.set(std::make_tuple(bundleIdentifier, origin), webClipIdentifier);
        dirty = true;
    }

    if (dirty)
        persist();

    return webClipIdentifier;
}

// Loads path as a plist and returns it if it has the shape [["a", "b", "c"], ["d", "e", "f"], ...]
static RetainPtr<NSArray> loadWebClipCachePropertyList(NSString *path)
{
    @autoreleasepool {
        NSError *error = nil;
        NSData *data = [NSData dataWithContentsOfFile:path options:NSDataReadingMappedIfSafe error:&error];
        if (!data) {
            RELEASE_LOG_ERROR(Push, "WebClipCache::load failed to read %{public}@: %{public}@", path, error);
            return nil;
        }

        id propertyList = [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListImmutable format:nullptr error:&error];
        if (!propertyList) {
            RELEASE_LOG_ERROR(Push, "WebClipCache::load failed to deserialize %{public}@: %{public}@", path, error);
            return nil;
        }

        if (![propertyList isKindOfClass:[NSArray class]]) {
            RELEASE_LOG_ERROR(Push, "WebClipCache::load failed to deserialize %{public}@: container isn't an array", path);
            return nil;
        }

        NSArray *entries = (NSArray *)propertyList;
        for (id entry in entries) {
            if (![entry isKindOfClass:[NSArray class]] || [entry count] != 3) {
                RELEASE_LOG_ERROR(Push, "WebClipCache::load failed to deserialize %{public}@: entry isn't an array", path);
                return nil;
            }

            NSArray *array = (NSArray *)entry;
            for (id val in array) {
                if (![val isKindOfClass:[NSString class]] || ![val length]) {
                    RELEASE_LOG_ERROR(Push, "WebClipCache::load failed to deserialize %{public}@: value isn't a string", path);
                    return nil;
                }
            }
        }

        return entries;
    }
}

void WebClipCache::load()
{
    RetainPtr entries = loadWebClipCachePropertyList(m_path);
    if (!entries)
        return;

    for (NSArray *entry in entries.get()) {
        NSString *bundleIdentifier = entry[0];
        NSString *securityOriginString = entry[1];
        NSString *webClipIdentifier = entry[2];

        RetainPtr securityOriginURL = adoptNS([[NSURL alloc] initWithString:securityOriginString]);
        auto origin = WebCore::SecurityOriginData::fromURL(URL { securityOriginURL.get() });
        if (origin.isNull())
            continue;

        m_preferredWebClipIdentifiers.add(std::make_tuple(String { bundleIdentifier }, origin), String { webClipIdentifier });
    }
}

void WebClipCache::persist()
{
    @autoreleasepool {
        RetainPtr<NSMutableArray> entries = adoptNS([[NSMutableArray alloc] init]);
        for (const auto& [bundleIdentifierAndOrigin, webClipIdentifier] : m_preferredWebClipIdentifiers) {
            NSString *bundleIdentifier = (NSString *)std::get<0>(bundleIdentifierAndOrigin);
            NSURL *url = (NSURL *)std::get<1>(bundleIdentifierAndOrigin).toURL();
            NSString *securityOriginString = [url absoluteString];
            if (!securityOriginString)
                continue;
            [entries addObject:@[bundleIdentifier, securityOriginString, (NSString *)webClipIdentifier]];
        }

        NSError *error = nil;
        NSData *data = [NSPropertyListSerialization dataWithPropertyList:entries.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
        if (!data) {
            RELEASE_LOG_ERROR(Push, "WebClipCache::persist failed to serialize plist: %{public}@", error);
            return;
        }

        if (![data writeToFile:(NSString *)m_path options:NSDataWritingAtomic error:&error]) {
            RELEASE_LOG_ERROR(Push, "WebClipCache::persist failed to write to disk: %{public}@", error);
            return;
        }
    }
}

HashSet<String> WebClipCache::visibleWebClipIdentifiers(const String& bundleIdentifier)
{
    HashSet<String> result;

    @autoreleasepool {
        NSArray *webClips = [UIWebClip webClips];

        for (UIWebClip *webClip in webClips) {
            NSString *identifier = webClip.identifier;
            if (!identifier)
                continue;

            // FIXME: Remove -respondsToSelector: check once we finish staging rdar://131961097.
            if (bundleIdentifier == "com.apple.SafariViewService"_s || ![webClip respondsToSelector:@selector(trustedClientBundleIdentifiers)] || [webClip.trustedClientBundleIdentifiers containsObject:(NSString *)bundleIdentifier]) {
                result.add(String { identifier });
                continue;
            }
        }
    }

    return result;
}

bool WebClipCache::isWebClipVisible(const String& bundleIdentifier, const String& webClipIdentifier)
{
    if (!webClipExists(webClipIdentifier))
        return false;

    if (bundleIdentifier == "com.apple.SafariViewService"_s)
        return true;

    UIWebClip *webClip = [UIWebClip webClipWithIdentifier:webClipIdentifier];
    if (![webClip respondsToSelector:@selector(trustedClientBundleIdentifiers)])
        return true;

    return [webClip.trustedClientBundleIdentifiers containsObject:(NSString *)bundleIdentifier];
}

} // namespace WebPushD

#endif
