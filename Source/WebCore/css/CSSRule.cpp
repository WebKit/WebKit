/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSRule.h"

#include "CSSCharsetRule.h"
#include "CSSFontFaceRule.h"
#include "CSSImportRule.h"
#include "CSSMediaRule.h"
#include "CSSPageRule.h"
#include "CSSStyleRule.h"
#include "CSSUnknownRule.h"
#include "WebKitCSSKeyframeRule.h"
#include "WebKitCSSKeyframesRule.h"
#include "WebKitCSSRegionRule.h"
#include "NotImplemented.h"

namespace WebCore {

void CSSRule::setCssText(const String& /*cssText*/, ExceptionCode& /*ec*/)
{
    notImplemented();
}

String CSSRule::cssText() const
{
    switch (type()) {
    case UNKNOWN_RULE:
        return String();
    case STYLE_RULE:
    case PAGE_RULE:
        return static_cast<const CSSStyleRule*>(this)->cssText();
    case CHARSET_RULE:
        return static_cast<const CSSCharsetRule*>(this)->cssText();
    case IMPORT_RULE:
        return static_cast<const CSSImportRule*>(this)->cssText();
    case MEDIA_RULE:
        return static_cast<const CSSMediaRule*>(this)->cssText();
    case FONT_FACE_RULE:
        return static_cast<const CSSFontFaceRule*>(this)->cssText();
    case WEBKIT_KEYFRAMES_RULE:
        return static_cast<const WebKitCSSKeyframesRule*>(this)->cssText();
    case WEBKIT_KEYFRAME_RULE:
        return static_cast<const WebKitCSSKeyframeRule*>(this)->cssText();
    case WEBKIT_REGION_RULE:
        return static_cast<const WebKitCSSRegionRule*>(this)->cssText();
    }
    ASSERT_NOT_REACHED();
    return String();
}

void CSSRule::destroy()
{
    switch (type()) {
    case UNKNOWN_RULE:
        delete static_cast<CSSUnknownRule*>(this);
        return;
    case STYLE_RULE:
        delete static_cast<CSSStyleRule*>(this);
        return;
    case PAGE_RULE:
        delete static_cast<CSSPageRule*>(this);
        return;
    case CHARSET_RULE:
        delete static_cast<CSSCharsetRule*>(this);
        return;
    case IMPORT_RULE:
        delete static_cast<CSSImportRule*>(this);
        return;
    case MEDIA_RULE:
        delete static_cast<CSSMediaRule*>(this);
        return;
    case FONT_FACE_RULE:
        delete static_cast<CSSFontFaceRule*>(this);
        return;
    case WEBKIT_KEYFRAMES_RULE:
        delete static_cast<WebKitCSSKeyframesRule*>(this);
        return;
    case WEBKIT_KEYFRAME_RULE:
        delete static_cast<WebKitCSSKeyframeRule*>(this);
        return;
    case WEBKIT_REGION_RULE:
        delete static_cast<WebKitCSSRegionRule*>(this);
        return;
    }
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
