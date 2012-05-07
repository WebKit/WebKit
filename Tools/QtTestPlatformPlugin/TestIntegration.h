/*
 * Copyright (C) 2012 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TestIntegration_h
#define TestIntegration_h

#include <QPlatformFontDatabase>
#include <QPlatformIntegration>
#include <QPlatformScreen>

class TestIntegration : public QPlatformIntegration {
public:
    TestIntegration();
private:
    virtual QPlatformFontDatabase *fontDatabase() const;

    // Redirecting everything else.
    virtual bool hasCapability(Capability cap) { return m_integration->hasCapability(cap); }
    virtual QPlatformPixmap* createPlatformPixmap(QPlatformPixmap::PixelType type) const { return m_integration->createPlatformPixmap(type); }
    virtual QPlatformWindow* createPlatformWindow(QWindow *window) const { return m_integration->createPlatformWindow(window); }
    virtual QPlatformBackingStore* createPlatformBackingStore(QWindow *window) const { return m_integration->createPlatformBackingStore(window); }
#ifndef QT_NO_OPENGL
    virtual QPlatformOpenGLContext* createPlatformOpenGLContext(QOpenGLContext *context) const { return m_integration->createPlatformOpenGLContext(context); }
#endif
    virtual QPlatformSharedGraphicsCache* createPlatformSharedGraphicsCache(const char *cacheId) const { return m_integration->createPlatformSharedGraphicsCache(cacheId); }
    virtual QAbstractEventDispatcher* guiThreadEventDispatcher() const { return m_integration->guiThreadEventDispatcher(); }
#ifndef QT_NO_CLIPBOARD
    virtual QPlatformClipboard* clipboard() const { return m_integration->clipboard(); }
#endif
#ifndef QT_NO_DRAGANDDROP
    virtual QPlatformDrag* drag() const { return m_integration->drag(); }
#endif
    virtual QPlatformInputContext* inputContext() const { return m_integration->inputContext(); }
    virtual QPlatformAccessibility* accessibility() const { return m_integration->accessibility(); }
    virtual QPlatformNativeInterface* nativeInterface() const { return m_integration->nativeInterface(); }
    virtual QPlatformServices* services() const { return m_integration->services(); }
    virtual QVariant styleHint(StyleHint hint) const { return m_integration->styleHint(hint); }
    virtual QPlatformTheme* createPlatformTheme(const QString& name) const { return m_integration->createPlatformTheme(name); }

    QScopedPointer<QPlatformIntegration> m_integration;
#if defined(Q_OS_MAC)
    mutable QScopedPointer<QPlatformFontDatabase> m_fontDatabase;
#endif
};

#endif
