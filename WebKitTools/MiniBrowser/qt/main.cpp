/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
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

#include "BrowserWindow.h"
#include <QLatin1String>
#include <QRegExp>
#include <qgraphicswkview.h>
#include <QtGui>

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QStringList args = QApplication::arguments();
    args.removeAt(0);

    QGraphicsWKView::BackingStoreType backingStoreTypeToUse = QGraphicsWKView::Simple;
    int indexOfTiledOption;
    if ((indexOfTiledOption = args.indexOf(QRegExp(QLatin1String("-tiled")))) != -1) {
        backingStoreTypeToUse = QGraphicsWKView::Tiled;
        args.removeAt(indexOfTiledOption);
    }

    if (args.isEmpty()) {
        QString defaultUrl = QString("file://%1/%2").arg(QDir::homePath()).arg(QLatin1String("index.html"));
        if (QDir(defaultUrl).exists())
            args.append(defaultUrl);
        else
            args.append("http://www.google.com");
    }

    BrowserWindow* window = new BrowserWindow(backingStoreTypeToUse);
    window->load(args[0]);

    for (int i = 1; i < args.size(); ++i)
        window->newWindow(args[i]);

    app.exec();

    return 0;
}
