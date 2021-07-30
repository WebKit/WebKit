/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ProcessWarming.h"

#include "CommonVM.h"
#include "Font.h"
#include "FontCache.h"
#include "FontCascadeDescription.h"
#include "HTMLNames.h"
#include "MathMLNames.h"
#include "MediaFeatureNames.h"
#include "QualifiedName.h"
#include "SVGNames.h"
#include "TelephoneNumberDetector.h"
#include "UserAgentStyle.h"
#include "WebKitFontFamilyNames.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"

#if ENABLE(GPU_DRIVER_PREWARMING)
#include "GPUDevice.h"
#endif

namespace WebCore {

void ProcessWarming::initializeNames()
{
    AtomString::init();
    HTMLNames::init();
    QualifiedName::init();
    MediaFeatureNames::init();
    SVGNames::init();
    XLinkNames::init();
    MathMLNames::init();
    XMLNSNames::init();
    XMLNames::init();
    WebKitFontFamilyNames::init();
}

void ProcessWarming::prewarmGlobally()
{
    initializeNames();
    
    // Prewarms user agent stylesheet.
    Style::UserAgentStyle::initDefaultStyleSheet();
    
    // Prewarms JS VM.
    commonVM();

    // Prewarm font cache
    FontCache::singleton().prewarmGlobally();

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
    TelephoneNumberDetector::prewarm();
#endif

#if ENABLE(GPU_DRIVER_PREWARMING)
    prewarmGPU();
#endif
}

WebCore::PrewarmInformation ProcessWarming::collectPrewarmInformation()
{
    return { FontCache::singleton().collectPrewarmInformation() };
}

void ProcessWarming::prewarmWithInformation(const PrewarmInformation& prewarmInfo)
{
    FontCache::singleton().prewarm(prewarmInfo.fontCache);
}

}
