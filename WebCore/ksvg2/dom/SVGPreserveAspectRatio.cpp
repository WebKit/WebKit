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

#include <kdom/ecma/Ecma.h>
#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "ksvg.h"

#include "SVGPreserveAspectRatio.h"
#include "SVGPreserveAspectRatioImpl.h"

#include "SVGConstants.h"
#include "SVGPreserveAspectRatio.lut.h"
using namespace KSVG;

/*
@begin SVGPreserveAspectRatio::s_hashTable 3
 align            SVGPreserveAspectRatioConstants::Align            DontDelete
 meetOrSlice    SVGPreserveAspectRatioConstants::MeetOrSlice    DontDelete
@end
*/

ValueImp *SVGPreserveAspectRatio::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGPreserveAspectRatioConstants::Align:
            return Number(align());
        case SVGPreserveAspectRatioConstants::MeetOrSlice:
            return Number(meetOrSlice());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(KDOM::DOMException)
    return Undefined();
}

void SVGPreserveAspectRatio::putValueProperty(ExecState *exec, int token, ValueImp *value, int)
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGPreserveAspectRatioConstants::Align:
            setAlign(value->toUInt32(exec));
            break;
        case SVGPreserveAspectRatioConstants::MeetOrSlice:
            setMeetOrSlice(value->toUInt32(exec));
            break;
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(KDOM::DOMException)
}

SVGPreserveAspectRatio SVGPreserveAspectRatio::null;

SVGPreserveAspectRatio::SVGPreserveAspectRatio() : impl(0)
{
}

SVGPreserveAspectRatio::SVGPreserveAspectRatio(SVGPreserveAspectRatioImpl *i)
: impl(i)
{
    if(impl)
        impl->ref();
}

SVGPreserveAspectRatio::SVGPreserveAspectRatio(const SVGPreserveAspectRatio &other) : impl(0)
{
    (*this) = other;
}

SVGPreserveAspectRatio::~SVGPreserveAspectRatio()
{
    if(impl)
    {
        if(impl->refCount() == 1)
            KDOM::ScriptInterpreter::forgetDOMObject(impl);

        impl->deref();
    }
}

SVGPreserveAspectRatio &SVGPreserveAspectRatio::operator=(const SVGPreserveAspectRatio &other)
{
    if(impl != other.impl)
    {
        if(impl)
        {
            if(impl->refCount() == 1)
                KDOM::ScriptInterpreter::forgetDOMObject(impl);

            impl->deref();
        }

        impl = other.impl;

        if(impl)
            impl->ref();
    }

    return *this;
}

bool SVGPreserveAspectRatio::operator==(const SVGPreserveAspectRatio &other) const
{
    return impl == other.impl;
}

bool SVGPreserveAspectRatio::operator!=(const SVGPreserveAspectRatio &other) const
{
    return !operator==(other);
}

void SVGPreserveAspectRatio::setAlign(unsigned short align)
{
    if(impl)
        impl->setAlign(align);
}

unsigned short SVGPreserveAspectRatio::align() const
{
    if(!impl)
        return SVG_PRESERVEASPECTRATIO_UNKNOWN;

    return impl->align();
}

void SVGPreserveAspectRatio::setMeetOrSlice(unsigned short meetOrSlice)
{
    if(impl)
        impl->setMeetOrSlice(meetOrSlice);
}

unsigned short SVGPreserveAspectRatio::meetOrSlice() const
{
    if(!impl)
        return SVG_MEETORSLICE_UNKNOWN;

    return impl->meetOrSlice();
}

// vim:ts=4:noet
