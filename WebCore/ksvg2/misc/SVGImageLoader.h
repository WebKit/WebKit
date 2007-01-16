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

#ifndef SVGImageLoader_h
#define SVGImageLoader_h
#ifdef SVG_SUPPORT

#include "HTMLImageLoader.h"

namespace WebCore {

    class SVGImageElement;

    class SVGImageLoader : public HTMLImageLoader {
    public:
        SVGImageLoader(SVGImageElement*);
        virtual ~SVGImageLoader();
        
        virtual void updateFromElement();
        virtual void dispatchLoadEvent();
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // SVGImageLoader_h

// vim:ts=4:noet
