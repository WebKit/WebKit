/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 University of Szeged
 *
 * All rights reserved.
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

#include "urlloader.h"

#include <QFile>
#include <QDebug>

UrlLoader::UrlLoader(QWebFrame* frame, const QString& inputFileName)
    : m_frame(frame)
    , m_stdOut(stdout)
    , m_loaded(0)
{
    init(inputFileName);
}

void UrlLoader::loadNext()
{
    QString qstr;
    if (getUrl(qstr)) {
        QUrl url(qstr, QUrl::StrictMode);
        if (url.isValid()) {
            m_stdOut << "Loading " << qstr << " ......" << ++m_loaded << endl;
            m_frame->load(url);
        } else
            loadNext();
    } else
        disconnect(m_frame, 0, this, 0);
}

void UrlLoader::init(const QString& inputFileName)
{
    QFile inputFile(inputFileName);
    if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&inputFile);
        QString line;
        while (true) {
            line = stream.readLine();
            if (line.isNull())
                break;
            m_urls.append(line);
        }
    } else {
        qDebug() << "Can't open list file";
        exit(0);
    }
    m_index = 0;
    inputFile.close();
}

bool UrlLoader::getUrl(QString& qstr)
{
    if (m_index == m_urls.size())
        return false;

    qstr = m_urls[m_index++];
    return true;
}
