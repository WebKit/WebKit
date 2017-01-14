/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include "LengthBox.h"
#include "RenderStyleConstants.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class StyleVisualData : public RefCounted<StyleVisualData> {
public:
    static Ref<StyleVisualData> create() { return adoptRef(*new StyleVisualData); }
    Ref<StyleVisualData> copy() const;
    ~StyleVisualData();

    bool operator==(const StyleVisualData& other) const
    {
        return clip == other.clip
            && hasClip == other.hasClip
            && textDecoration == other.textDecoration
            && zoom == other.zoom;
    }
    bool operator!=(const StyleVisualData& other) const { return !(*this == other); }

    LengthBox clip;
    bool hasClip : 1;
    unsigned textDecoration : TextDecorationBits; // Text decorations defined *only* by this element.
    float zoom;

private:
    StyleVisualData();
    StyleVisualData(const StyleVisualData&);    
};

} // namespace WebCore
