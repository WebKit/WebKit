/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "KWQWidget.h"
#include "KWQMap.h"
#include "KWQString.h"

class KJavaAppletContext;
class KJavaAppletWidget;

#ifdef __OBJC__
@class NSMutableDictionary;
#else
class NSMutableDictionary;
#endif

class KJavaApplet
{
public:
    KJavaApplet(KJavaAppletWidget &widget) : m_widget(widget) { }

    void setAppletClass(const QString &) { }
    void setAppletName(const QString &) { }
    void setArchives(const QString &) { }
    void setBaseURL(const QString &);
    void setCodeBase(const QString &) { }
    
    void setParameter(const QString &, const QString &);

private:
    KJavaAppletWidget &m_widget;
};

class KJavaAppletWidget : public QWidget
{
public:
    KJavaAppletWidget(KJavaAppletContext *, QWidget *);
    ~KJavaAppletWidget();
    
    void processArguments(const QMap<QString, QString>&);

    KJavaApplet *applet() { return &_applet; }
    
    void setBaseURL(const QString &baseURL) { _baseURL = baseURL; }
    void setParameter(const QString &, const QString &);

    void showApplet();

private:
    KJavaApplet _applet;
    KJavaAppletContext *_context;
    QString _baseURL;
    NSMutableDictionary *_parameters;
};

inline void KJavaApplet::setBaseURL(const QString &URL) { m_widget.setBaseURL(URL); }
inline void KJavaApplet::setParameter(const QString &name, const QString &value) { m_widget.setParameter(name, value); }
