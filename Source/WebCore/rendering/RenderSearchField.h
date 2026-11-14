/*
 * Copyright (C) 2006, 2007, 2009, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#include "RenderTextControlSingleLine.h"

namespace WebCore {

class HTMLInputElement;

class RenderSearchField final : public RenderTextControlSingleLine {
    WTF_MAKE_TZONE_ALLOCATED(RenderSearchField);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSearchField);
public:
    RenderSearchField(HTMLInputElement&, Style::ComputedStyle&&);
    virtual ~RenderSearchField();

#if PLATFORM(WIN)
    int clientInsetRight() const;
    int clientInsetLeft() const;
    LayoutUnit clientPaddingRight() const;
    LayoutUnit clientPaddingLeft() const;
#endif

private:
    LayoutUnit computeControlLogicalHeight(LayoutUnit lineHeight, LayoutUnit nonContentHeight) const override;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSearchField, isRenderSearchField())
