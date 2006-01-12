/*
    Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>

    This file is part of the WebKit project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_SVGImageLoader_H
#define KSVG_SVGImageLoader_H

#include "khtml/html/html_imageimpl.h"

namespace KSVG
{
    class SVGImageElementImpl;
    class SVGImageLoader : public DOM::HTMLImageLoader {
    public:
        SVGImageLoader(SVGImageElementImpl *node);
        virtual ~SVGImageLoader();
        
        virtual void updateFromElement();
    };
};

#endif

// vim:ts=4:noet
