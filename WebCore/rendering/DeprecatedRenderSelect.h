/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DeprecatedRenderSelect_h
#define DeprecatedRenderSelect_h

#include "RenderFormElement.h"

namespace WebCore {
    
    class HTMLSelectElement;
    class ListBox;

    class DeprecatedRenderSelect : public RenderFormElement {
    public:
        DeprecatedRenderSelect(HTMLSelectElement*);

        virtual const char* renderName() const { return "DeprecatedRenderSelect"; }

        short baselinePosition(bool f, bool b) const;
        int calcReplacedHeight() const { if (!m_useListBox) return intrinsicHeight(); return RenderFormElement::calcReplacedHeight(); }
        virtual bool canHaveIntrinsicMargins() const { return true; }

        virtual void calcMinMaxWidth();
        virtual void layout();

        void setOptionsChanged(bool);

        bool selectionChanged() { return m_selectionChanged; }
        void setSelectionChanged(bool selectionChanged) { m_selectionChanged = selectionChanged; }
        virtual void updateFromElement();
        virtual void setStyle(RenderStyle*);

        void updateSelection();

    private:
        virtual void selectionChanged(Widget*);

    protected:
        ListBox* createListBox();
        void setWidgetWritingDirection();

        unsigned m_size;
        bool m_multiple;
        bool m_useListBox;
        bool m_selectionChanged;
        bool m_ignoreSelectEvents;
        bool m_optionsChanged;
    };

} // namespace WebCore

#endif // DeprecatedRenderSelect_h
