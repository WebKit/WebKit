/*
 * Copyright (C) 2003, 2007, 2010, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DisabledAdaptations.h"
#include <wtf/Function.h>
#include <wtf/OptionSet.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FloatRect;

struct WindowFeatures {
    std::optional<float> x;
    std::optional<float> y;
    std::optional<float> width;
    std::optional<float> height;

    bool menuBarVisible { true };
    bool statusBarVisible { true };
    bool toolBarVisible { true };
    bool locationBarVisible { true };
    bool scrollbarsVisible { true };
    bool resizable { true };

    bool fullscreen { false };
    bool dialog { false };
    bool noopener { false };

    Vector<String> additionalFeatures;
};

WindowFeatures parseWindowFeatures(StringView windowFeaturesString);
WindowFeatures parseDialogFeatures(const String& dialogFeaturesString, const FloatRect& screenAvailableRect);
OptionSet<DisabledAdaptations> parseDisabledAdaptations(const String&);

enum class FeatureMode { Window, Viewport };
void processFeaturesString(StringView features, FeatureMode, const WTF::Function<void(StringView type, StringView value)>& callback);

} // namespace WebCore
