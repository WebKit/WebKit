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

#ifndef KDOM_LSParserImpl_H
#define KDOM_LSParserImpl_H

#include <kdom/events/EventTargetImpl.h>

class QBuffer;
class KURL;

namespace KDOM
{
    class NodeImpl;
    class DOMConfigurationImpl;
    class DocumentImpl;
    class Parser;
    class LSInputImpl;
    class LSParserFilterImpl;

    class LSParserImpl : public EventTargetImpl
    {
    public:
        LSParserImpl();
        virtual ~LSParserImpl();

        // 'LSParser' functions
        DOMConfigurationImpl *domConfig() const;

        LSParserFilterImpl *filter() const;
        void setFilter(LSParserFilterImpl *filter);

        /**
         * true if the LSParser is asynchronous, false if it is synchronous. 
         */
        bool async() const;
        // not in spec
        void setASync(bool async);

        bool busy() const;

        DocumentImpl *parse(LSInputImpl *input);
        DocumentImpl *parseURI(DOMStringImpl *uri);

        NodeImpl *parseWithContext(LSInputImpl *input, NodeImpl *contextArg,
                                   unsigned short action);

        void abort() const;

    private:
        DocumentImpl *parse(KURL url, LSInputImpl *input, bool async, NodeImpl *contextArg = 0);

    private:
        bool m_async;
        Parser *m_activeParser;
        mutable DOMConfigurationImpl *m_config;
    };
};

#endif

// vim:ts=4:noet
