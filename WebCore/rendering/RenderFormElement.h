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

#ifndef RenderFormElement_h
#define RenderFormElement_h

#include "GraphicsTypes.h"
#include "RenderWidget.h"

namespace WebCore {

    class HTMLGenericFormElement;

    class RenderFormElement : public RenderWidget {
    public:
        RenderFormElement(HTMLGenericFormElement*);
        virtual ~RenderFormElement();

        virtual const char* renderName() const { return "RenderForm"; }

        virtual bool isFormElement() const { return true; }

        // Aqua form controls never have border/padding.
        int borderTop() const { return 0; }
        int borderBottom() const { return 0; }
        int borderLeft() const { return 0; }
        int borderRight() const { return 0; }
        int paddingTop() const { return 0; }
        int paddingBottom() const { return 0; }
        int paddingLeft() const { return 0; }
        int paddingRight() const { return 0; }

        virtual void setStyle(RenderStyle*);
        virtual void updateFromElement();

        virtual void layout();
        virtual short baselinePosition(bool, bool) const;

    protected:
        virtual bool isEditable() const { return false; }

        HorizontalAlignment textAlignment() const;

    private:
        virtual void clicked(Widget*);
    };

} // namespace WebCore

#endif // RenderFormElement_h
