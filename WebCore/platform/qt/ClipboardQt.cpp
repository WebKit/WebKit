/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ClipboardQt.h"

#include "NotImplemented.h"
#include "DeprecatedString.h"
#include "Document.h"
#include "Frame.h"
#include "IntPoint.h"
#include "KURL.h"
#include "PlatformString.h"
#include "Range.h"
#include "StringHash.h"
#include <QList>
#include <QMimeData>
#include <QStringList>
#include <QUrl>


namespace WebCore {
    
ClipboardQt::ClipboardQt(ClipboardAccessPolicy policy, const QMimeData* readableClipboard, bool forDragging)
    : Clipboard(policy, forDragging)
    , m_readableData(readableClipboard)
    , m_writableData(0)
{
    ASSERT(m_readableData);
}    

ClipboardQt::ClipboardQt(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
{
    m_writableData = new QMimeData();
    m_readableData = m_writableData;    
}

ClipboardQt::~ClipboardQt()
{
    if (m_writableData)
        delete m_writableData;
    m_readableData = 0;
}

void ClipboardQt::clearData(const String& type)
{
    ASSERT(m_writableData);
    notImplemented();
}

void ClipboardQt::clearAllData() 
{
    ASSERT(m_writableData);
    m_writableData->clear();
}

String ClipboardQt::getData(const String& type, bool& success) const 
{
    ASSERT(m_writableData);
    QByteArray data = m_writableData->data(QString(type));
    success = !data.isEmpty();
    return String(data.data(), data.size());
}

bool ClipboardQt::setData(const String& type, const String& data) 
{
    ASSERT(m_writableData);
    QByteArray array(reinterpret_cast<const char*>(data.characters()),
                     data.length());
    m_writableData->setData(QString(type), array);
    return true;
}

// extensions beyond IE's API
HashSet<String> ClipboardQt::types() const
{
    HashSet<String> result;
    QStringList formats = m_writableData->formats();
    for (int i = 0; i < formats.count(); ++i) {
        String type(formats.at(i).toLatin1().data());
        result.add(type);
    }
    return result;
}

IntPoint ClipboardQt::dragLocation() const 
{ 
    notImplemented();
    return IntPoint(0,0);
}

CachedImage* ClipboardQt::dragImage() const 
{
    notImplemented();
    return 0; 
}

void ClipboardQt::setDragImage(CachedImage*, const IntPoint&) 
{
    notImplemented();
}

Node* ClipboardQt::dragImageElement() 
{
    notImplemented();
    return 0; 
}

void ClipboardQt::setDragImageElement(Node*, const IntPoint&)
{
    notImplemented();
}

DragImageRef ClipboardQt::createDragImage(IntPoint& dragLoc) const
{ 
    notImplemented();
    return 0;
}

void ClipboardQt::declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*) 
{
    ASSERT(m_writableData);
    notImplemented();
}

void ClipboardQt::writeURL(const KURL& url, const String&, Frame* frame) 
{
    ASSERT(m_writableData);
    QList<QUrl> urls;
    urls.append(QUrl(frame->document()->completeURL(url.url())));
    m_writableData->setUrls(urls);
}

void ClipboardQt::writeRange(Range* range, Frame*) 
{
    ASSERT(m_writableData);
    m_writableData->setText(range->text());
    m_writableData->setHtml(range->toHTML());
}

bool ClipboardQt::hasData() 
{
    return m_readableData->formats().count() > 0;
}

}
