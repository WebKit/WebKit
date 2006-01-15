/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef _XML_Tokenizer_h_
#define _XML_Tokenizer_h_

#include <qobject.h>
#include <qmap.h>
#include "SegmentedString.h"

#include "KWQSignal.h"

class KHTMLView;

namespace WebCore {
    class SegmentedString;
};

namespace DOM {
    class DocumentImpl;
    class DocumentFragmentImpl;
    class ElementImpl;
    class NodeImpl;
};

namespace khtml {

class Tokenizer : public QObject
{
    Q_OBJECT

public:
    Tokenizer();
    // script output must be prepended, while new data
    // received during executing a script must be appended, hence the
    // extra bool to be able to distinguish between both cases. document.write()
    // always uses false, while khtmlpart uses true
    virtual bool write(const SegmentedString &str, bool appendData) = 0;
    virtual void finish() = 0;
    virtual void setOnHold(bool onHold) = 0;
    virtual bool isWaitingForScripts() const = 0;
    virtual void stopParsing() { m_parserStopped = true; }
    virtual bool processingData() const { return false; }

protected:
    // The tokenizer has buffers, so parsing may continue even after
    // it stops receiving data. We use m_parserStopped to stop the tokenizer
    // even when it has buffered data.
    bool m_parserStopped;
    
#ifdef KHTML_XSLT
public:
    virtual void setTransformSource(DOM::DocumentImpl* doc) {};
#endif

signals:
    void finishedParsing();

private:
    KWQSignal m_finishedParsing;
};

Tokenizer *newXMLTokenizer(DOM::DocumentImpl *, KHTMLView * = 0);
#ifdef KHTML_XSLT
void *xmlDocPtrForString(const QString &source, const QString &url = QString());
#endif
QMap<QString, QString> parseAttributes(const DOM::DOMString &, bool &attrsOK);
bool parseXMLDocumentFragment(const DOM::DOMString &, DOM::DocumentFragmentImpl *, DOM::ElementImpl *parent = 0);
}

#endif
