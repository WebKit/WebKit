/*
 * Copyright (C) 2002 Apple Computer, Inc.  All rights reserved.
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

#include <khtml_part.h>

#ifdef __OBJC__
@class IFWebDataSource;
#else
class IFWebDataSource;
#endif

class KWQKHTMLPartImpl : public QObject
{
public:
    KWQKHTMLPartImpl(KHTMLPart *);
    ~KWQKHTMLPartImpl();
    
    void setView(KHTMLView *view);

    bool openURLInFrame(const KURL &, const KParts::URLArgs &);
    void openURL(const KURL &);
    void begin(const KURL &, int xOffset, int yOffset);
    void write(const char *str, int len);
    void end();
    
    void slotData(NSString *, const char *bytes, int length, bool complete = false);

    void scheduleRedirection(int delay, const QString &url);
    void redirectURL();
    virtual void timerEvent(QTimerEvent *);

    bool gotoBaseAnchor();

    void setBaseURL(const KURL &);

    void setTitle(const DOM::DOMString &);
    
    QString documentSource() const;

    void setDataSource(IFWebDataSource *);
    IFWebDataSource *getDataSource();

    bool frameExists(const QString &frameName);

    void urlSelected(const QString &url, int button, int state, const QString &_target, KParts::URLArgs);
    bool requestFrame(khtml::RenderPart *frame, const QString &url, const QString &frameName, const QStringList &params, bool isIFrame);
    bool requestObject(khtml::RenderPart *frame, const QString &url, const QString &serviceType, const QStringList &args);

    void submitForm(const char *action, const QString &url, const QByteArray &formData, const QString &_target, const QString& contentType, const QString& boundary);

private:
    KHTMLPart *part;
    KHTMLPartPrivate *d;

    int m_redirectionTimer;
    
    KURL m_baseURL;
    QString m_documentSource;
    bool m_decodingStarted;
    IFWebDataSource *m_dataSource;
    
    friend class KHTMLPart;
};
