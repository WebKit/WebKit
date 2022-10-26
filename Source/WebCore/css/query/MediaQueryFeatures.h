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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GenericMediaQueryTypes.h"

namespace WebCore::MQ {

namespace Features {

const FeatureSchema& animation();
const FeatureSchema& anyHover();
const FeatureSchema& anyPointer();
const FeatureSchema& aspectRatio();
const FeatureSchema& color();
const FeatureSchema& colorGamut();
const FeatureSchema& colorIndex();
const FeatureSchema& deviceAspectRatio();
const FeatureSchema& deviceHeight();
const FeatureSchema& devicePixelRatio();
const FeatureSchema& deviceWidth();
const FeatureSchema& dynamicRange();
const FeatureSchema& forcedColors();
const FeatureSchema& grid();
const FeatureSchema& height();
const FeatureSchema& hover();
const FeatureSchema& invertedColors();
const FeatureSchema& monochrome();
const FeatureSchema& orientation();
const FeatureSchema& pointer();
const FeatureSchema& prefersContrast();
const FeatureSchema& prefersDarkInterface();
const FeatureSchema& prefersReducedMotion();
const FeatureSchema& resolution();
const FeatureSchema& scan();
const FeatureSchema& transform2d();
const FeatureSchema& transform3d();
const FeatureSchema& transition();
const FeatureSchema& videoPlayableInline();
const FeatureSchema& width();
#if ENABLE(APPLICATION_MANIFEST)
const FeatureSchema& displayMode();
#endif
#if ENABLE(DARK_MODE_CSS)
const FeatureSchema& prefersColorScheme();
#endif

Vector<const FeatureSchema*> allSchemas();

};

}

