/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef CSSHelper_h
#define CSSHelper_h

namespace WebCore {

    class String;

    // Used in many inappropriate contexts throughout WebCore. We'll have to examine and test
    // each call site to find out whether it needs the various things this function does. That
    // includes trimming leading and trailing control characters (including whitespace), removing
    // url() or URL() if it surrounds the entire string, removing matching quote marks if present,
    // and stripping all characters in the range U+0000-U+000C. Probably no caller needs this.
    String deprecatedParseURL(const String&);

    // We always assume 96 CSS pixels in a CSS inch. This is the cold hard truth of the Web.
    // At high DPI, we may scale a CSS pixel, but the ratio of the CSS pixel to the so-called
    // "absolute" CSS length units like inch and pt is always fixed and never changes.
    const float cssPixelsPerInch = 96;

} // namespace WebCore

#endif // CSSHelper_h
