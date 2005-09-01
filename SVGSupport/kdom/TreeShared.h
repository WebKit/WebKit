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

#ifndef KDOM_TreeShared_H
#define KDOM_TreeShared_H

#include <kdom/Shared.h>

namespace KDOM
{
    template<class T> class TreeShared : public Shared
    {
    public:
        TreeShared() : Shared() { m_parent = 0; }
        virtual ~TreeShared() { }

        void setParent(T *parent) { m_parent = parent; }
        T *parent() const { return m_parent; }

        virtual void deref()
        {
            if(m_ref)
                m_ref--; 

            if(!m_ref && !m_parent)
                delete this;
        }

    protected:
        T *m_parent;
    };
};

#endif

// vim:ts=4:noet
