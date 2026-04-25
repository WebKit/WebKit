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

namespace WebCore {

UITraitCollection *traitCollectionWithAdjustedIdiomForSystemColors(UITraitCollection *traitCollection)
{
#if PLATFORM(VISION)
    // Use the iPad idiom instead of the Vision idiom, since some system colors are transparent
    // in the Vision idiom, and are not web-compatible.
    if (traitCollection.userInterfaceIdiom == UIUserInterfaceIdiomVision) {
        return [traitCollection traitCollectionByModifyingTraits:^(id<UIMutableTraits> traits) {
            traits.userInterfaceIdiom = UIUserInterfaceIdiomPad;
        }];
    }
#endif
    return traitCollection;
}

LocalCurrentTraitCollection::LocalCurrentTraitCollection(bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
    m_savedTraitCollection = [PAL::getUITraitCollectionClassSingleton() currentTraitCollection];

    // FIXME: <rdar://problem/96607991> `-[UITraitCollection currentTraitCollection]` is not guaranteed
    // to return a useful set of traits in cases where it has not been explicitly set. Ideally, this
    // method should also take in a base, full-specified trait collection from the view hierarchy, to be
    // used when building the new trait collection.
    RetainPtr combinedTraits = [m_savedTraitCollection traitCollectionByModifyingTraits:^(id<UIMutableTraits> traits) {
        traits.userInterfaceStyle = useDarkAppearance ? UIUserInterfaceStyleDark : UIUserInterfaceStyleLight;
        traits.userInterfaceLevel = useElevatedUserInterfaceLevel ? UIUserInterfaceLevelElevated : UIUserInterfaceLevelBase;
    }];

    [PAL::getUITraitCollectionClassSingleton() setCurrentTraitCollection:traitCollectionWithAdjustedIdiomForSystemColors(combinedTraits.get())];
}

LocalCurrentTraitCollection::LocalCurrentTraitCollection(UITraitCollection *traitCollection)
{
    m_savedTraitCollection = [PAL::getUITraitCollectionClassSingleton() currentTraitCollection];
    auto newTraitCollection = traitCollectionWithAdjustedIdiomForSystemColors(traitCollection);
    [PAL::getUITraitCollectionClassSingleton() setCurrentTraitCollection:newTraitCollection];
}

LocalCurrentTraitCollection::~LocalCurrentTraitCollection()
{
    [PAL::getUITraitCollectionClassSingleton() setCurrentTraitCollection:m_savedTraitCollection.get()];
}

}

#endif // PLATFORM(IOS_FAMILY)
