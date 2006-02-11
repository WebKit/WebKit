/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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

#ifndef KSVG_SVGHelper_H
#define KSVG_SVGHelper_H
#if SVG_SUPPORT

#include <QString.h>
#include <kxmlcore/RefPtr.h>

namespace WebCore
{
    class SVGElementImpl;
    class SVGStringListImpl;

    // KSVG extension
    enum LengthMode
    {
        LM_UNKNOWN = 0,
        LM_WIDTH,
        LM_HEIGHT,
        LM_OTHER
    };

    class SVGHelper
    {
    public:
        static float PercentageOfViewport(float value, const SVGElementImpl *viewportElement, LengthMode mode);

        static void ParseSeperatedList(SVGStringListImpl *list, const QString &data, const QChar &delimiter = ',');
    };

    // Lazy creation, template-based
    template<class T>
    T *lazy_create(RefPtr<T> &variable)
    { 
        if(!variable)
            variable = new T();

        return variable.get();
    }

    template<class T, class Arg1>
    T *lazy_create(RefPtr<T> &variable, Arg1 arg1)
    { 
        if(!variable)
            variable = new T(arg1);

        return variable.get();
    }

    template<class T, class Arg1, class Arg2>
    T *lazy_create(RefPtr<T> &variable, Arg1 arg1, Arg2 arg2)
    { 
        if(!variable)
            variable = new T(arg1, arg2);
        
        return variable.get();
    }

    template<class T, class Arg1, class Arg2, class Arg3>
    T *lazy_create(RefPtr<T> &variable, Arg1 arg1, Arg2 arg2, Arg3 arg3)
    { 
        if(!variable)
            variable = new T(arg1, arg2, arg3);

        return variable.get();
    }

    template<class T, class Arg1, class Arg2, class Arg3, class Arg4>
    T *lazy_create(RefPtr<T> &variable, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4)
    { 
        if(!variable)
            variable = new T(arg1, arg2, arg3, arg4);

        return variable.get();
    }

    template<class T, class Arg1, class Arg2, class Arg3, class Arg4, class Arg5>
    T *lazy_create(RefPtr<T> &variable, Arg1 arg1, Arg2 arg2, Arg3 arg3, Arg4 arg4, Arg5 arg5)
    { 
        if(!variable)
            variable = new T(arg1, arg2, arg3, arg4, arg5);

        return variable.get();
    }
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
