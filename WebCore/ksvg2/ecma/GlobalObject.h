/*
    Copyright (C) 2002 David Faure <faure@kde.org>
                     2004, 2005 Nikolas Zimmermann <wildfox@kde.org>

    This file is part of the KDE project

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2, as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this program; see the file COPYING.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_GlobalObject_H
#define KSVG_GlobalObject_H
#if SVG_SUPPORT

#include <kdom/ecma/GlobalObject.h>

namespace KDOM
{
    class DocumentImpl;
}

namespace KSVG
{
    class GlobalObject : public KDOM::GlobalObject
    {
    public:
        GlobalObject(KDOM::DocumentImpl *doc);
        virtual ~GlobalObject();

        virtual void afterTimeout() const;
        virtual KJS::JSValue *get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const;

        // EcmaScript specific stuff - only needed for GlobalObject
        // You won't find it in "general" kdom ecma code...
        static const struct KJS::HashTable s_hashTable;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
