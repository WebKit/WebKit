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

#ifndef KDOM_DOMErrorImpl_H
#define KDOM_DOMErrorImpl_H

#include <kdom/Shared.h>

namespace KDOM
{
    class DOMStringImpl;
    class DOMObjectImpl;
    class DOMLocatorImpl;

    enum ErrorSeverity
    {
        SEVERITY_WARNING = 1,
        SEVERITY_ERROR = 2,
        SEVERITY_FATAL_ERROR = 3
    };

    // Introduced in DOM Level 3:
    class DOMErrorImpl : public Shared<DOMErrorImpl>
    {
    public:
        DOMErrorImpl();
        virtual ~DOMErrorImpl();

        unsigned short severity() const;

        DOMStringImpl *message() const;
        DOMStringImpl *type() const;

        DOMObjectImpl *relatedException() const;
        DOMObjectImpl *relatedData() const;

        DOMLocatorImpl *location() const;

        // Helpers
        void setSeverity(unsigned short severity);

        void setMessage(DOMStringImpl *message);
        void setType(DOMStringImpl *type);

        void setRelatedException(DOMObjectImpl *relatedException);
        void setRelatedData(DOMObjectImpl *relatedData);

        void setLocation(DOMLocatorImpl *location);

    protected:
        unsigned short m_severity;
        DOMStringImpl *m_message;
        DOMStringImpl *m_type;
        DOMObjectImpl *m_relatedException;
        DOMObjectImpl *m_relatedData;
        mutable DOMLocatorImpl *m_location;
    };

};

#endif

// vim:ts=4:noet
