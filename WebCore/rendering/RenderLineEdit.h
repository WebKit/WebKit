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

#ifndef RenderLineEdit_h
#define RenderLineEdit_h

#include "RenderFormElement.h"

namespace WebCore {

    class HTMLInputElement;

    class RenderLineEdit : public RenderFormElement {
    public:
        RenderLineEdit(HTMLInputElement*);

        virtual const char* renderName() const { return "RenderLineEdit"; }

        virtual void calcMinMaxWidth();
        int calcReplacedHeight() const { return intrinsicHeight(); }
        virtual bool canHaveIntrinsicMargins() const { return true; }

        virtual void updateFromElement();
        virtual void setStyle(RenderStyle*);

        int selectionStart();
        int selectionEnd();
        void setSelectionStart(int);
        void setSelectionEnd(int);
        
        bool isEdited() const;
        void setEdited(bool);
        bool isTextField() const { return true; }
        void select();
        void setSelectionRange(int, int);

        void addSearchResult();

    private:
        virtual void returnPressed(Widget*);
        virtual void valueChanged(Widget*);
        virtual void selectionChanged(Widget*);
        virtual void performSearch(Widget*);

        virtual bool isEditable() const { return true; }

        bool m_updating;
    };

} // namespace WebCore

#endif // RenderLineEdit_h
