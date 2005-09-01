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

#include "ksvg2/ecma/Ecma.h"
#include "SVGPathSeg.h"
#include "SVGException.h"
#include "SVGElementImpl.h"
#include "SVGPathSegList.h"
#include "SVGExceptionImpl.h"
#include "SVGPathSegListImpl.h"

#include "SVGConstants.h"
#include "SVGPathSegList.lut.h"
using namespace KSVG;
using namespace KDOM;

/*
@begin SVGPathSegList::s_hashTable 3
 numberOfItems        SVGPathSegListConstants::NumberOfItems    DontDelete|ReadOnly
@end
@begin SVGPathSegListProto::s_hashTable 9
 clear                SVGPathSegListConstants::Clear            DontDelete|Function 0
 initialize            SVGPathSegListConstants::Initialize        DontDelete|Function 1
 getItem            SVGPathSegListConstants::GetItem            DontDelete|Function 1
 insertItemBefore    SVGPathSegListConstants::InsertItemBefore    DontDelete|Function 2
 replaceItem        SVGPathSegListConstants::ReplaceItem        DontDelete|Function 2
 removeItem            SVGPathSegListConstants::RemoveItem        DontDelete|Function 1
 appendItem            SVGPathSegListConstants::AppendItem        DontDelete|Function 1
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGPathSegList", SVGPathSegListProto, SVGPathSegListProtoFunc)

ValueImp *SVGPathSegList::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGPathSegListConstants::NumberOfItems:
            return Number(numberOfItems());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

ValueImp *SVGPathSegListProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    KDOM_CHECK_THIS(SVGPathSegList)
    KDOM_ENTER_SAFE

    switch(id)
    {
        case SVGPathSegListConstants::Clear:
        {
            obj.clear();
            return Undefined();
        }
        case SVGPathSegListConstants::Initialize:
        {
            SVGPathSeg newItem = KDOM::ecma_cast<SVGPathSeg>(exec, args[0], &toSVGPathSeg);
            return getSVGPathSeg(exec, obj.initialize(newItem));
        }
        case SVGPathSegListConstants::GetItem:
        {
            unsigned long index = args[0]->toUInt32(exec);
            return getSVGPathSeg(exec, obj.getItem(index));
        }
        case SVGPathSegListConstants::InsertItemBefore:
        {
            SVGPathSeg newItem = KDOM::ecma_cast<SVGPathSeg>(exec, args[0], &toSVGPathSeg);
            unsigned long index = args[1]->toUInt32(exec);
            return getSVGPathSeg(exec, obj.insertItemBefore(newItem, index));
        }
        case SVGPathSegListConstants::ReplaceItem:
        {
            SVGPathSeg newItem = KDOM::ecma_cast<SVGPathSeg>(exec, args[0], &toSVGPathSeg);
            unsigned long index = args[1]->toUInt32(exec);
            return getSVGPathSeg(exec, obj.replaceItem(newItem, index));
        }
        case SVGPathSegListConstants::RemoveItem:
        {
            unsigned long index = args[0]->toUInt32(exec);
            return getSVGPathSeg(exec, obj.removeItem(index));
        }
        case SVGPathSegListConstants::AppendItem:
        {
            SVGPathSeg newItem = KDOM::ecma_cast<SVGPathSeg>(exec, args[0], &toSVGPathSeg);
            return getSVGPathSeg(exec, obj.appendItem(newItem));
        }
        default:
            kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
    }

    KDOM_LEAVE_CALL_SAFE(SVGException)
    return Undefined();
}
SVGPathSegList SVGPathSegList::null;

SVGPathSegList::SVGPathSegList() : impl(0)
{
}

SVGPathSegList::SVGPathSegList(SVGPathSegListImpl *i) : impl(i)
{
    if(impl)
        impl->ref();
}

SVGPathSegList::SVGPathSegList(const SVGPathSegList &other) : impl(0)
{
    (*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGPathSegList)

unsigned long SVGPathSegList::numberOfItems() const
{
    if(!impl)
        return 0;

    return impl->numberOfItems();
}

void SVGPathSegList::clear()
{
    if(impl)
        impl->clear();
}

SVGPathSeg SVGPathSegList::initialize(SVGPathSeg newItem)
{
    if(!impl)
        return SVGPathSeg::null;

    return SVGPathSeg(impl->initialize(newItem.handle()));
}

SVGPathSeg SVGPathSegList::getItem(unsigned long index)
{
    if(!impl)
        return SVGPathSeg::null;

    return SVGPathSeg(impl->getItem(index));
}

SVGPathSeg SVGPathSegList::insertItemBefore(SVGPathSeg newItem, unsigned long index)
{
    if(!impl)
        return SVGPathSeg::null;

    return SVGPathSeg(impl->insertItemBefore(newItem.handle(), index));
}

SVGPathSeg SVGPathSegList::replaceItem(SVGPathSeg newItem, unsigned long index)
{
    if(!impl)
        return SVGPathSeg::null;

    return SVGPathSeg(impl->replaceItem(newItem.handle(), index));
}

SVGPathSeg SVGPathSegList::removeItem(unsigned long index)
{
    if(!impl)
        return SVGPathSeg::null;

    return SVGPathSeg(impl->removeItem(index));
}

SVGPathSeg SVGPathSegList::appendItem(SVGPathSeg newItem)
{
    if(!impl)
        return SVGPathSeg::null;

    return SVGPathSeg(impl->appendItem(newItem.handle()));
}

// vim:ts=4:noet
