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

#include "DOMImplementationLSImpl.h"

#include "kdom.h"
#include "kdomls.h"
#include "Namespace.h"
#include "LSInputImpl.h"
#include "LSOutputImpl.h"
#include "LSParserImpl.h"
#include "DOMStringImpl.h"
#include "DOMExceptionImpl.h"
#include "LSSerializerImpl.h"

using namespace KDOM;

DOMImplementationLSImpl::DOMImplementationLSImpl()
{
}

DOMImplementationLSImpl::~DOMImplementationLSImpl()
{
}

LSParserImpl *DOMImplementationLSImpl::createLSParser(unsigned short mode, DOMStringImpl *schemaTypeImpl) const
{
    QString schemaType = schemaTypeImpl ? schemaTypeImpl->string() : QString();

    // NOT_SUPPORTED_ERR: Raised if the requested mode or schema type is not supported.
    if((mode == 0 || mode > MODE_ASYNCHRONOUS) ||
        !(schemaType == NS_SCHEMATYPE_DTD || schemaType == NS_SCHEMATYPE_WXS || schemaType.isEmpty()))
        throw new DOMExceptionImpl(NOT_SUPPORTED_ERR);

    LSParserImpl *ret = new LSParserImpl();
    ret->setASync(mode == MODE_ASYNCHRONOUS);
    return ret;
}

LSInputImpl *DOMImplementationLSImpl::createLSInput() const
{
    return new LSInputImpl();
}

LSOutputImpl *DOMImplementationLSImpl::createLSOutput() const
{
    return new LSOutputImpl();
}

LSSerializerImpl *DOMImplementationLSImpl::createLSSerializer() const
{
    return new LSSerializerImpl();
}

// vim:ts=4:noet
