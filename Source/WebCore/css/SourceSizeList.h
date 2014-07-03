/*
 * Copyright (C) 2014 Yoav Weiss <yoav@yoav.ws>
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

#ifndef SourceSizeList_h
#define SourceSizeList_h

#if ENABLE(PICTURE_SIZES)

#include "CSSParserValues.h"
#include "MediaQueryExp.h"
#include <memory>

namespace WebCore {

class RenderStyle;
class RenderView;
class Frame;

class SourceSize {
public:
    SourceSize(std::unique_ptr<MediaQueryExp> mediaExp, const CSSParserValue& length)
        : m_mediaExp(WTF::move(mediaExp))
        , m_length(length)
    {
    }

    bool match(RenderStyle&, Frame*);
    unsigned length(RenderStyle&, RenderView*);

private:
    std::unique_ptr<MediaQueryExp> m_mediaExp;
    CSSParserValue m_length;
};

class SourceSizeList {
public:
    void append(std::unique_ptr<SourceSize> sourceSize)
    {
        m_list.append(WTF::move(sourceSize));
    }

    static unsigned parseSizesAttribute(const String& sizesAttribute, RenderView*, Frame*);
    unsigned getEffectiveSize(RenderStyle&, RenderView*, Frame*);

private:
    Vector<std::unique_ptr<SourceSize>> m_list;
};

} // namespace WebCore

#endif // ENABLE(PICTURE_SIZES)

#endif // SourceSizeList_h

