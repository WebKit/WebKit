/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
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
 * $Id$
 */
#ifndef RENDER_HR_H
#define RENDER_HR_H

#include "rendering/render_box.h"

namespace khtml {

class RenderHR : public RenderBox
{
public:
    RenderHR();
    virtual ~RenderHR();

    virtual const char *renderName() const { return "RenderHR"; }

    virtual bool isRendered() const { return true; }

    virtual void calcMinMaxWidth();

    void print(QPainter *p, int _x, int _y, int _w, int _h,
               int _tx, int _ty);
    
    virtual void layout();

protected:
    short m_length;
    bool hr_shade;
};


}; //namespace

#endif
