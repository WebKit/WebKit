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

#ifndef KDOM_LSInputImpl_H
#define KDOM_LSInputImpl_H

#include <kdom/Shared.h>

class QTextIStream;

namespace KDOM
{
    class DOMString;
    class DOMStringImpl;

    class LSInputImpl : public Shared<LSInputImpl>
    {
    public:
        LSInputImpl();
        virtual ~LSInputImpl();

        // 'LSInput' functions
        QTextIStream *characterStream() const;
        void setCharacterStream(QTextIStream *characterStream);

        DOMStringImpl *byteStream() const;
        void setByteStream(DOMStringImpl *byteStream);

        DOMStringImpl *stringData() const;
        void setStringData(DOMStringImpl *stringData);

        DOMStringImpl *systemId() const;
        void setSystemId(DOMStringImpl *systemId);

        DOMStringImpl *publicId() const;
        void setPublicId(DOMStringImpl *publicId);

        DOMStringImpl *baseURI() const;
        void setBaseURI(DOMStringImpl *baseURI);

        DOMStringImpl *encoding() const;
        void setEncoding(DOMStringImpl *encoding);

        bool certifiedText() const;
        void setCertifiedText(bool certifiedText);

    private:
        QTextIStream *m_characterStream;
        DOMStringImpl *m_byteStream;
        DOMStringImpl *m_stringData;
        DOMStringImpl *m_systemId;
        DOMStringImpl *m_publicId;
        DOMStringImpl *m_baseURI;
        DOMStringImpl *m_encoding;
        bool m_certifiedText : 1;
    };
};

#endif

// vim:ts=4:noet
