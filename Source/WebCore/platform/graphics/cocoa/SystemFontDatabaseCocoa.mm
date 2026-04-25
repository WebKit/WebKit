/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "SystemFontDatabaseCoreText.h"

#import <pal/ios/UIKitSoftLink.h>

namespace WebCore {

static auto cocoaFontClassSingleton()
{
#if PLATFORM(IOS_FAMILY)
    return PAL::getUIFontClassSingleton();
#else
    return NSFont.class;
#endif
};

RetainPtr<CTFontDescriptorRef> SystemFontDatabaseCoreText::smallCaptionFontDescriptor()
{
    auto font = [cocoaFontClassSingleton() systemFontOfSize:[cocoaFontClassSingleton() smallSystemFontSize]];
    return static_cast<CTFontDescriptorRef>(font.fontDescriptor);
}

RetainPtr<CTFontDescriptorRef> SystemFontDatabaseCoreText::menuFontDescriptor()
{
    return adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontMenuItem, [cocoaFontClassSingleton() systemFontSize], nullptr));
}

RetainPtr<CTFontDescriptorRef> SystemFontDatabaseCoreText::statusBarFontDescriptor()
{
    return adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, [cocoaFontClassSingleton() labelFontSize], nullptr));
}

RetainPtr<CTFontDescriptorRef> SystemFontDatabaseCoreText::miniControlFontDescriptor()
{
#if PLATFORM(IOS_FAMILY)
    return adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontMiniSystem, 0, nullptr));
#else
    auto font = [cocoaFontClassSingleton() systemFontOfSize:[cocoaFontClassSingleton() systemFontSizeForControlSize:NSControlSizeMini]];
    return static_cast<CTFontDescriptorRef>(font.fontDescriptor);
#endif
}

RetainPtr<CTFontDescriptorRef> SystemFontDatabaseCoreText::smallControlFontDescriptor()
{
#if PLATFORM(IOS_FAMILY)
    return adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSmallSystem, 0, nullptr));
#else
    auto font = [cocoaFontClassSingleton() systemFontOfSize:[cocoaFontClassSingleton() systemFontSizeForControlSize:NSControlSizeSmall]];
    return static_cast<CTFontDescriptorRef>(font.fontDescriptor);
#endif
}

RetainPtr<CTFontDescriptorRef> SystemFontDatabaseCoreText::controlFontDescriptor()
{
#if PLATFORM(IOS_FAMILY)
    return adoptCF(CTFontDescriptorCreateForUIType(kCTFontUIFontSystem, 0, nullptr));
#else
    auto font = [cocoaFontClassSingleton() systemFontOfSize:[cocoaFontClassSingleton() systemFontSizeForControlSize:NSControlSizeRegular]];
    return static_cast<CTFontDescriptorRef>(font.fontDescriptor);
#endif
}

} // namespace WebCore
