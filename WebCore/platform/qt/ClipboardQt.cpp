/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "CachedImage.h"
#include "CSSHelper.h"
#include "Document.h"
#include "Element.h"
#include "FileList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Image.h"
#include "IntPoint.h"
#include "KURL.h"
#include "markup.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "Range.h"
#include "RenderImage.h"
#include "StringHash.h"

#include <QList>
#include <QMimeData>
#include <QStringList>
#include <QUrl>
#include <QApplication>
#include <QClipboard>
#include <qdebug.h>

#define methodDebug() qDebug("ClipboardQt: %s", __FUNCTION__)

namespace WebCore {

ClipboardQt::ClipboardQt(ClipboardAccessPolicy policy, const QMimeData* readableClipboard)
    : Clipboard(policy, true)
    , m_readableData(readableClipboard)
    , m_writableData(0)
{
    Q_ASSERT(policy == ClipboardReadable || policy == ClipboardTypesReadable);
}

ClipboardQt::ClipboardQt(ClipboardAccessPolicy policy, bool forDragging)
    : Clipboard(policy, forDragging)
    , m_readableData(0)
    , m_writableData(0)
{
    Q_ASSERT(policy == ClipboardReadable || policy == ClipboardWritable || policy == ClipboardNumb);

#ifndef QT_NO_CLIPBOARD
    if (policy != ClipboardWritable) {
        Q_ASSERT(!forDragging);
        m_readableData = QApplication::clipboard()->mimeData();
    }
#endif
}

ClipboardQt::~ClipboardQt()
{
    if (m_writableData && !isForDragging())
        m_writableData = 0;
    else
        delete m_writableData;
    m_readableData = 0;
}

void ClipboardQt::clearData(const String& type)
{
    if (policy() != ClipboardWritable)
        return;

    if (m_writableData) {
#if QT_VERSION >= 0x040400
        m_writableData->removeFormat(type);
#else
        const QString toClearType = type;
        QMap<QString, QByteArray> formats;
        foreach (QString format, m_writableData->formats()) {
            if (format != toClearType)
                formats[format] = m_writableData->data(format);
        }

        m_writableData->clear();
        QMap<QString, QByteArray>::const_iterator it, end = formats.constEnd();
        for (it = formats.begin(); it != end; ++it)
            m_writableData->setData(it.key(), it.value());
#endif
        if (m_writableData->formats().isEmpty()) {
            if (isForDragging())
                delete m_writableData;
            m_writableData = 0;
        }
    }
#ifndef QT_NO_CLIPBOARD
    if (!isForDragging())
        QApplication::clipboard()->setMimeData(m_writableData);
#endif
}

void ClipboardQt::clearAllData()
{
    if (policy() != ClipboardWritable)
        return;

#ifndef QT_NO_CLIPBOARD
    if (!isForDragging())
        QApplication::clipboard()->setMimeData(0);
    else
#endif
        delete m_writableData;
    m_writableData = 0;
}

String ClipboardQt::getData(const String& type, bool& success) const
{

    if (policy() != ClipboardReadable) {
        success = false;
        return String();
    }

    ASSERT(m_readableData);
    QByteArray data = m_readableData->data(QString(type));
    success = !data.isEmpty();
    return String(data.data(), data.size());
}

bool ClipboardQt::setData(const String& type, const String& data)
{
    if (policy() != ClipboardWritable)
        return false;

    if (!m_writableData)
        m_writableData = new QMimeData;
    QByteArray array(reinterpret_cast<const char*>(data.characters()),
                     data.length()*2);
    m_writableData->setData(QString(type), array);
#ifndef QT_NO_CLIPBOARD
    if (!isForDragging())
        QApplication::clipboard()->setMimeData(m_writableData);
#endif
    return true;
}

// extensions beyond IE's API
HashSet<String> ClipboardQt::types() const
{
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return HashSet<String>();

    ASSERT(m_readableData);
    HashSet<String> result;
    QStringList formats = m_readableData->formats();
    for (int i = 0; i < formats.count(); ++i)
        result.add(formats.at(i));
    return result;
}

PassRefPtr<FileList> ClipboardQt::files() const
{
    notImplemented();
    return 0;
}

void ClipboardQt::setDragImage(CachedImage* image, const IntPoint& point)
{
    setDragImage(image, 0, point);
}

void ClipboardQt::setDragImageElement(Node* node, const IntPoint& point)
{
    setDragImage(0, node, point);
}

void ClipboardQt::setDragImage(CachedImage* image, Node *node, const IntPoint &loc)
{
    if (policy() != ClipboardImageWritable && policy() != ClipboardWritable)
        return;

    if (m_dragImage)
        m_dragImage->removeClient(this);
    m_dragImage = image;
    if (m_dragImage)
        m_dragImage->addClient(this);

    m_dragLoc = loc;
    m_dragImageElement = node;
}

DragImageRef ClipboardQt::createDragImage(IntPoint& dragLoc) const
{
    if (!m_dragImage)
        return 0;
    dragLoc = m_dragLoc;
    return m_dragImage->image()->nativeImageForCurrentFrame();
}


static CachedImage* getCachedImage(Element* element)
{
    // Attempt to pull CachedImage from element
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;

    RenderImage* image = toRenderImage(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return 0;
}

void ClipboardQt::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    ASSERT(frame);

    //WebCore::writeURL(m_writableDataObject.get(), url, title, true, false);
    if (!m_writableData)
        m_writableData = new QMimeData;

    CachedImage* cachedImage = getCachedImage(element);
    if (!cachedImage || !cachedImage->image() || !cachedImage->isLoaded())
        return;
    QPixmap *pixmap = cachedImage->image()->nativeImageForCurrentFrame();
    if (pixmap)
        m_writableData->setImageData(*pixmap);

    AtomicString imageURL = element->getAttribute(HTMLNames::srcAttr);
    if (imageURL.isEmpty())
        return;

    KURL fullURL = frame->document()->completeURL(deprecatedParseURL(imageURL));
    if (fullURL.isEmpty())
        return;

    QList<QUrl> urls;
    urls.append(url);
    urls.append(fullURL);

    m_writableData->setText(title);
    m_writableData->setUrls(urls);
#ifndef QT_NO_CLIPBOARD
    if (!isForDragging())
        QApplication::clipboard()->setMimeData(m_writableData);
#endif
}

void ClipboardQt::writeURL(const KURL& url, const String& title, Frame* frame)
{
    ASSERT(frame);

    QList<QUrl> urls;
    urls.append(frame->document()->completeURL(url.string()));
    if (!m_writableData)
        m_writableData = new QMimeData;
    m_writableData->setUrls(urls);
    m_writableData->setText(title);
#ifndef QT_NO_CLIPBOARD
    if (!isForDragging())
        QApplication::clipboard()->setMimeData(m_writableData);
#endif
}

void ClipboardQt::writeRange(Range* range, Frame* frame)
{
    ASSERT(range);
    ASSERT(frame);

    if (!m_writableData)
        m_writableData = new QMimeData;
    QString text = frame->selectedText();
    text.replace(QChar(0xa0), QLatin1Char(' '));
    m_writableData->setText(text);
    m_writableData->setHtml(createMarkup(range, 0, AnnotateForInterchange));
#ifndef QT_NO_CLIPBOARD
    if (!isForDragging())
        QApplication::clipboard()->setMimeData(m_writableData);
#endif
}

bool ClipboardQt::hasData()
{
    const QMimeData *data = m_readableData ? m_readableData : m_writableData;
    if (!data)
        return false;
    return data->formats().count() > 0;
}

}
