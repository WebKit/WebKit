/*
 * This file is part of the HTML rendering engine for KDE.
 *
 * Copyright (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006 Apple Computer, Inc.
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
#ifndef RenderCounter_h
#define RenderCounter_h

#include "RenderText.h"

namespace WebCore {

class CounterNode;

class RenderCounter : public RenderText {
public:
    RenderCounter(Node*, CounterData*);

    virtual const char* renderName() const { return "RenderCounter"; }

    virtual bool isCounter() const { return true; }

    virtual void layout();
    virtual void calcMinMaxWidth();

private:
    String convertValueToType(int value, int total, EListStyleType);

    String m_item;
    CounterData* m_counter;
    CounterNode* m_counterNode;
};

} // namespace WebCore

#endif // RenderCounter_h
