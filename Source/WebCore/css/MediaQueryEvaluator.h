/*
 * CSS Media Query Evaluator
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaQueryEvaluator_h
#define MediaQueryEvaluator_h

#include "PlatformString.h"

namespace WebCore {

class Frame;
class MediaQueryExp;
class MediaQuerySet;
class RenderStyle;
class StyleResolver;

// Class that evaluates CSS media queries as defined in CSS3 Module "Media Queries".
//
// Special constructors are supplied so that simple media queries can be
// evaluated without knowledge of device characteristics. This is used, for example,
// when parsing user agent stylesheets. The boolean parameter to the constructor is used
// if the device characteristics are not known. This can be used to prune loading
// of stylesheets to remove those that definitely won't match.

class MediaQueryEvaluator {
     WTF_MAKE_NONCOPYABLE(MediaQueryEvaluator); WTF_MAKE_FAST_ALLOCATED;
public:
    // Creates evaluator which evaluates only simple media queries.
    // Evaluator returns true for acceptedMediaType and uses mediaFeatureResult for any media features.
    MediaQueryEvaluator(const String& acceptedMediaType, bool mediaFeatureResult = false);
    MediaQueryEvaluator(const char* acceptedMediaType, bool mediaFeatureResult = false);

    // Creates evaluator which evaluates full media queries.
    MediaQueryEvaluator(const String& acceptedMediaType, Frame*, RenderStyle*);

    ~MediaQueryEvaluator();

    bool mediaTypeMatch(const String& mediaTypeToMatch) const;
    bool mediaTypeMatchSpecific(const char* mediaTypeToMatch) const;

    // Evaluates a list of media queries.
    bool eval(const MediaQuerySet*, StyleResolver* = 0) const;

    // Evaluates media query subexpression, i.e. "and (media-feature: value)" part.
    bool eval(const MediaQueryExp&) const;

private:
    String m_mediaType;
    Frame* m_frame; // not owned
    RefPtr<RenderStyle> m_style;
    bool m_mediaFeatureResult;
};

} // namespace WebCore

#endif
