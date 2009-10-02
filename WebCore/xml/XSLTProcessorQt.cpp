/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2009 Jakub Wieczorek <faw217@gmail.com>
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(XSLT)

#include "XSLTProcessor.h"

#include "Console.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "TransformSource.h"
#include "loader.h"
#include "markup.h"
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

#include <qabstractmessagehandler.h>
#include <qbuffer.h>
#include <qsourcelocation.h>
#include <qxmlquery.h>

namespace WebCore {

class XSLTMessageHandler : public QAbstractMessageHandler {

public:
    XSLTMessageHandler(Document* document = 0);
    virtual void handleMessage(QtMsgType type, const QString& description,
                               const QUrl& identifier, const QSourceLocation& sourceLocation);

private:
    Document* m_document;
};

XSLTMessageHandler::XSLTMessageHandler(Document* document)
    : QAbstractMessageHandler()
    , m_document(document)
{
}

void XSLTMessageHandler::handleMessage(QtMsgType type, const QString& description,
                                       const QUrl&, const QSourceLocation& sourceLocation)
{
    if (!m_document->frame())
        return;

    MessageLevel level;
    switch (type) {
    case QtDebugMsg:
        level = TipMessageLevel;
        break;
    case QtWarningMsg:
        level = WarningMessageLevel;
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = ErrorMessageLevel;
        break;
    default:
        level = LogMessageLevel;
        break;
    }

    Console* console = m_document->frame()->domWindow()->console();
    console->addMessage(XMLMessageSource, LogMessageType, level, description,
                        sourceLocation.line(), sourceLocation.uri().toString());
}

bool XSLTProcessor::transformToString(Node* sourceNode, String&, String& resultString, String&)
{
    bool success = false;

    RefPtr<XSLStyleSheet> stylesheet = m_stylesheet;
    if (!stylesheet && m_stylesheetRootNode) {
        Node* node = m_stylesheetRootNode.get();
        stylesheet = XSLStyleSheet::create(node->parent() ? node->parent() : node, node->document()->url().string());
        stylesheet->parseString(createMarkup(node));
    }

    if (!stylesheet || stylesheet->sheetString().isEmpty())
        return success;

    RefPtr<Document> ownerDocument = sourceNode->document();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());

    QXmlQuery query(QXmlQuery::XSLT20);

    XSLTMessageHandler messageHandler(ownerDocument.get());
    query.setMessageHandler(&messageHandler);

    XSLTProcessor::ParameterMap::iterator end = m_parameters.end();
    for (XSLTProcessor::ParameterMap::iterator it = m_parameters.begin(); it != end; ++it)
        query.bindVariable(QString(it->first), QXmlItem(QVariant(it->second)));

    QString source;
    if (sourceIsDocument && ownerDocument->transformSource())
        source = ownerDocument->transformSource()->platformSource();
    if (!sourceIsDocument || source.isEmpty())
        source = createMarkup(sourceNode);

    QBuffer inputBuffer;
    QBuffer styleSheetBuffer;
    QBuffer outputBuffer;

    inputBuffer.setData(source.toUtf8());
    styleSheetBuffer.setData(QString(stylesheet->sheetString()).toUtf8());

    inputBuffer.open(QIODevice::ReadOnly);
    styleSheetBuffer.open(QIODevice::ReadOnly);
    outputBuffer.open(QIODevice::ReadWrite);

    query.setFocus(&inputBuffer);
    query.setQuery(&styleSheetBuffer, QUrl(stylesheet->href()));
    success = query.evaluateTo(&outputBuffer);
    outputBuffer.reset();
    resultString = QString::fromUtf8(outputBuffer.readAll()).trimmed();

    if (m_stylesheet) {
        m_stylesheet->clearDocuments();
        m_stylesheet = 0;
    }

    return success;
}

} // namespace WebCore

#endif // ENABLE(XSLT)
