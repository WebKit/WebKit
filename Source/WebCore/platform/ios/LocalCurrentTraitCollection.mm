/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "LocalCurrentTraitCollection.h"

#if PLATFORM(IOS_FAMILY)

#import <pal/ios/UIKitSoftLink.h>

#if HAVE(OS_DARK_MODE_SUPPORT)
#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/LocalCurrentTraitCollectionAdditions.mm>)
#import <WebKitAdditions/LocalCurrentTraitCollectionAdditions.mm>
#else
namespace WebCore {

static UITraitCollection *adjustedTraitCollection(UITraitCollection *traitCollection)
{
    return traitCollection;
}

}
#endif
#endif // HAVE(OS_DARK_MODE_SUPPORT)


namespace WebCore {

LocalCurrentTraitCollection::LocalCurrentTraitCollection(bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    m_savedTraitCollection = [PAL::getUITraitCollectionClass() currentTraitCollection];

    auto userInterfaceStyleTrait = [PAL::getUITraitCollectionClass() traitCollectionWithUserInterfaceStyle:useDarkAppearance ? UIUserInterfaceStyleDark : UIUserInterfaceStyleLight];
    auto backgroundLevelTrait = [PAL::getUITraitCollectionClass() traitCollectionWithUserInterfaceLevel:useElevatedUserInterfaceLevel ? UIUserInterfaceLevelElevated : UIUserInterfaceLevelBase];

    // FIXME: <rdar://problem/96607991> `-[UITraitCollection currentTraitCollection]` is not guaranteed
    // to return a useful set of traits in cases where it has not been explicitly set. Ideally, this
    // method should also take in a base, full-specified trait collection from the view hierarchy, to be
    // used when building the new trait collection.
    auto newTraitCollection = adjustedTraitCollection([PAL::getUITraitCollectionClass() traitCollectionWithTraitsFromCollections:@[ m_savedTraitCollection.get(), userInterfaceStyleTrait, backgroundLevelTrait ]]);

    [PAL::getUITraitCollectionClass() setCurrentTraitCollection:newTraitCollection];
#else
    UNUSED_PARAM(useDarkAppearance);
    UNUSED_PARAM(useElevatedUserInterfaceLevel);
#endif
}

LocalCurrentTraitCollection::LocalCurrentTraitCollection(UITraitCollection *traitCollection)
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    m_savedTraitCollection = [PAL::getUITraitCollectionClass() currentTraitCollection];
    auto newTraitCollection = adjustedTraitCollection(traitCollection);
    [PAL::getUITraitCollectionClass() setCurrentTraitCollection:newTraitCollection];
#else
    UNUSED_PARAM(traitCollection);
#endif
}

LocalCurrentTraitCollection::~LocalCurrentTraitCollection()
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    [PAL::getUITraitCollectionClass() setCurrentTraitCollection:m_savedTraitCollection.get()];
#endif
}

}

#endif // PLATFORM(IOS_FAMILY)
