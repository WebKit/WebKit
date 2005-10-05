/*
 * This file is part of the KDE libraries
 *
 * Copyright (C) 2005 Frans Englich     <frans.englich@telia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include <kdebug.h>

#include "DOMStringImpl.h"
#include "NBCImpl.h"
#include "XPointerResultImpl.h"

#include "PointerPartImpl.h"

using namespace KDOM;
using namespace KDOM::XPointer;

PointerPartImpl::PointerPartImpl(DOMStringImpl *name, DOMStringImpl *schemeData, NBCImpl *nbc)
: Shared(), m_data(schemeData), m_name(name), m_nbc(nbc)
{
    Q_ASSERT(m_name);
    Q_ASSERT(m_data);
    kdDebug(26550) << "Created PointerPart: " << endl;
    kdDebug(26550) << "     name = \"" << name->string() << "\"" << endl;
    kdDebug(26550) << "     schemeData = \"" << (m_data ? schemeData->string() : QString()) << "\"" << endl;

    if(m_name)
        m_name->ref();
    if(m_data)
        m_data->ref();
    if(m_nbc)
        m_nbc->ref();
}

PointerPartImpl::~PointerPartImpl()
{
    if(m_name)
        m_name->deref();
    if(m_data)
        m_data->deref();
    if(m_nbc)
        m_nbc->deref();
}

XPointerResultImpl *PointerPartImpl::evaluate(NodeImpl *) const
{
    /* Unknown schemes are plain PointerPartImpl instances. This ensures
     * they trigger an evaluation of the next scheme. */
    return new XPointerResultImpl(NO_MATCH);
}

NBCImpl *PointerPartImpl::nbc() const
{
    return m_nbc;
}

DOMStringImpl *PointerPartImpl::name() const
{
    return m_name;
}

DOMStringImpl *PointerPartImpl::data() const
{
    return m_data;
}

// vim:ts=4:noet
