/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "SecurityFlagsController.h"

#import "Logging.h"
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
#import <pal/cocoa/WebPrivacySoftLink.h>
#endif

namespace WebKit {

#if (defined(ENGINEERING_BUILD) && ENGINEERING_BUILD) || ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

static Vector<String> flagNamesFrom(NSArray *names)
{
    return makeVector(names, [](id name) -> std::optional<String> {
        if (RetainPtr string = dynamic_objc_cast<NSString>(name))
            return String { string.get() };
        return std::nullopt;
    });
}

#endif

#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD

static NSString * const disabledSecurityFlagsDefaultsKey = @"WebKitDebugDisabledSecurityFlags";

Vector<String> SecurityFlagsController::platformPersistentlyDisabledFlagNames()
{
    RetainPtr value = [NSUserDefaults.standardUserDefaults objectForKey:disabledSecurityFlagsDefaultsKey];
    if (!value)
        return { };

    // A bare string rather than an array is what writing the default without -array produces.
    if (RetainPtr name = dynamic_objc_cast<NSString>(value.get()))
        return { String { name.get() } };

    RetainPtr names = dynamic_objc_cast<NSArray>(value.get());
    if (!names) {
        RELEASE_LOG_ERROR(Process, "SecurityFlagsController: ignoring %" PUBLIC_LOG_STRING ", which is neither an array of strings nor a string", disabledSecurityFlagsDefaultsKey.UTF8String);
        return { };
    }

    return flagNamesFrom(names.get());
}

#else

Vector<String> SecurityFlagsController::platformPersistentlyDisabledFlagNames()
{
    return { };
}

#endif // ENGINEERING_BUILD

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

bool SecurityFlagsController::platformRequestRemoteList(uint64_t generation)
{
    static bool canRequestDisabledFlagNames = PAL::isWebPrivacyFrameworkAvailable()
        && [PAL::getWPResourcesClassSingleton() instancesRespondToSelector:@selector(requestDisabledSecurityFlags:)];

    if (!canRequestDisabledFlagNames) {
        RELEASE_LOG_INFO(Process, "SecurityFlagsController: WebPrivacy cannot serve the disabled security flags, so every flag stays enforced");
        return false;
    }

    [[PAL::getWPResourcesClassSingleton() sharedInstance] requestDisabledSecurityFlags:^(NSArray<NSString *> *names, NSError *error) {
        // Which queue WebPrivacy answers on is its own choice, and the flags may only be written on the main one.
        ensureOnMainRunLoop([generation, names = RetainPtr { names }, error = RetainPtr { error }] {
            if (!names) {
                RELEASE_LOG_ERROR(Process, "SecurityFlagsController: WebPrivacy failed to answer with the disabled security flags: %{public}@", error.get());
                singleton().didReceiveRemoteList(generation, std::nullopt);
                return;
            }

            singleton().didReceiveRemoteList(generation, flagNamesFrom(names.get()));
        });
    }];

    return true;
}

void SecurityFlagsController::platformObserveRemoteListChanges()
{
    // First, because canLoad_ below loads the framework in the mode that release-asserts when it is not installed.
    if (!PAL::isWebPrivacyFrameworkAvailable())
        return;

    if (!PAL::canLoad_WebPrivacy_WPDisabledSecurityFlagsChangedNotificationName()) {
        RELEASE_LOG_INFO(Process, "SecurityFlagsController: WebPrivacy does not announce security flag changes, so a change will only be seen at the next launch");
        return;
    }

    static NeverDestroyed<RetainPtr<id>> observer;
    if (observer->get())
        return;

    observer.get() = [[NSNotificationCenter defaultCenter] addObserverForName:PAL::get_WebPrivacy_WPDisabledSecurityFlagsChangedNotificationNameSingleton() object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification *) {
        singleton().requestRemoteList();
    }];
}

#else

bool SecurityFlagsController::platformRequestRemoteList(uint64_t)
{
    return false;
}

void SecurityFlagsController::platformObserveRemoteListChanges()
{
}

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

} // namespace WebKit
