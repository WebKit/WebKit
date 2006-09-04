/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef WebKitPart_H
#define WebKitPart_H

#include <QObject>

#include <wtf/RefPtr.h>
#include <kparts/part.h>

namespace WebCore {
    class Frame;
    class FrameView;
}

class WebKitPartClient;

class WebKitPart : public KParts::ReadOnlyPart
{
Q_OBJECT
Q_PROPERTY(QString url READ url)  

public:
    enum GUIProfile {
        DefaultGUI,
        BrowserViewGUI
    };

    WebKitPart(QWidget* parentWidget = 0,
               QObject* parentObject = 0,
               GUIProfile prof = DefaultGUI);

    virtual ~WebKitPart();

    /**
     * Opens the specified URL @p url.
     *
     * Reimplemented from KParts::ReadOnlyPart::openURL .
     */
    virtual bool openUrl(const KUrl&);

    /**
     * Stops loading the document and kills all data requests (for images, etc.)
     */
    virtual bool closeUrl();

    /**
     * Returns a pointer to the parent WebKitPart
     * if the part is a frame in an HTML frameset.
     *
     * Returns 0 otherwise.
     */
    WebKitPart* parentPart();

private:
    friend class WebKitPartClient;
    
    /**
     * Returns pointer the current frame.
     */
    WebCore::Frame* frame();

    /**
     * Internal empty reimplementation of KParts::ReadOnlyPart::openFile .
     */
    virtual bool openFile();

    /**
     * Internal helper method
     */
    void initView(QWidget*, GUIProfile);

private:
    WTF::RefPtr<WebCore::Frame> m_frame;
    WTF::RefPtr<WebCore::FrameView> m_frameView;

    WebKitPartClient* m_client;
};

#endif
