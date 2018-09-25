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

#import "config.h"
#import "FontAttributes.h"

#import "ColorCocoa.h"
#import <wtf/SoftLinking.h>

namespace WebCore {

RetainPtr<NSDictionary> FontAttributes::createDictionary() const
{
    NSMutableDictionary *attributes = [NSMutableDictionary dictionary];
    if (font)
        attributes[NSFontAttributeName] = font.get();

    if (foregroundColor.isValid())
        attributes[NSForegroundColorAttributeName] = platformColor(foregroundColor);

    if (backgroundColor.isValid())
        attributes[NSBackgroundColorAttributeName] = platformColor(backgroundColor);

    if (fontShadow.color.isValid() && (!fontShadow.offset.isEmpty() || fontShadow.blurRadius)) {
        auto shadow = fontShadow.createShadow();
        attributes[NSShadowAttributeName] = shadow.get();
    }

    if (subscriptOrSuperscript == SubscriptOrSuperscript::Subscript)
        attributes[NSSuperscriptAttributeName] = @(-1);
    else if (subscriptOrSuperscript == SubscriptOrSuperscript::Superscript)
        attributes[NSSuperscriptAttributeName] = @1;

    if (hasUnderline)
        attributes[NSUnderlineStyleAttributeName] = @(NSUnderlineStyleSingle);

    if (hasStrikeThrough)
        attributes[NSStrikethroughStyleAttributeName] = @(NSUnderlineStyleSingle);

    return attributes;
}

} // namespace WebCore
