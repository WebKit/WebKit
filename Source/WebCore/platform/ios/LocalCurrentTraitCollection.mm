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

#include "config.h"
#include "LocalCurrentTraitCollection.h"

#if PLATFORM(IOS_FAMILY)

#include <pal/ios/UIKitSoftLink.h>

namespace WebCore {

LocalCurrentTraitCollection::LocalCurrentTraitCollection(bool useDarkAppearance, bool useElevatedUserInterfaceLevel)
{
#if HAVE(OS_DARK_MODE_SUPPORT)
    m_savedTraitCollection = [PAL::getUITraitCollectionClass() currentTraitCollection];
    m_usingDarkAppearance = useDarkAppearance;
    m_usingElevatedUserInterfaceLevel = useElevatedUserInterfaceLevel;

    auto userInterfaceStyleTrait = [PAL::getUITraitCollectionClass() traitCollectionWithUserInterfaceStyle:m_usingDarkAppearance ? UIUserInterfaceStyleDark : UIUserInterfaceStyleLight];
    auto backgroundLevelTrait = [PAL::getUITraitCollectionClass() traitCollectionWithUserInterfaceLevel:m_usingElevatedUserInterfaceLevel ? UIUserInterfaceLevelElevated : UIUserInterfaceLevelBase];
    auto newTraitCollection = [PAL::getUITraitCollectionClass() traitCollectionWithTraitsFromCollections:@[ m_savedTraitCollection.get(), userInterfaceStyleTrait, backgroundLevelTrait ]];

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
    m_usingDarkAppearance = traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
    m_usingElevatedUserInterfaceLevel = traitCollection.userInterfaceLevel == UIUserInterfaceLevelElevated;

    [PAL::getUITraitCollectionClass() setCurrentTraitCollection:traitCollection];
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
