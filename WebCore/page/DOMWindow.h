/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef DOMWindow_h
#define DOMWindow_h

#include "PlatformString.h"
#include <wtf/RefCounted.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class BarInfo;
    class CSSRuleList;
    class CSSStyleDeclaration;
    class Console;
    class DOMSelection;
    class Database;
    class Document;
    class Element;
    class FloatRect;
    class Frame;
    class History;
    class Screen;

    typedef int ExceptionCode;

    class DOMWindow : public RefCounted<DOMWindow> {
    public:
        DOMWindow(Frame*);
        virtual ~DOMWindow();

        Frame* frame() { return m_frame; }
        void disconnectFrame();

        void clear();

        static void adjustWindowRect(const FloatRect& screen, FloatRect& window, const FloatRect& pendingChanges);

        // DOM Level 0
        Screen* screen() const;
        History* history() const;
        BarInfo* locationbar() const;
        BarInfo* menubar() const;
        BarInfo* personalbar() const;
        BarInfo* scrollbars() const;
        BarInfo* statusbar() const;
        BarInfo* toolbar() const;

        DOMSelection* getSelection();

        Element* frameElement() const;

        void focus();
        void blur();
        void close();
        void print();
        void stop();

        void alert(const String& message);
        bool confirm(const String& message);
        String prompt(const String& message, const String& defaultValue);

        bool find(const String&, bool caseSensitive, bool backwards, bool wrap, bool wholeWord, bool searchInFrames, bool showDialog) const;

        bool offscreenBuffering() const;

        int outerHeight() const;
        int outerWidth() const;
        int innerHeight() const;
        int innerWidth() const;
        int screenX() const;
        int screenY() const;
        int screenLeft() const { return screenX(); }
        int screenTop() const { return screenY(); }
        int scrollX() const;
        int scrollY() const;
        int pageXOffset() const { return scrollX(); }
        int pageYOffset() const { return scrollY(); }

        bool closed() const;

        unsigned length() const;

        String name() const;
        void setName(const String&);

        String status() const;
        void setStatus(const String&);
        String defaultStatus() const;
        void setDefaultStatus(const String&);
        // This attribute is an alias of defaultStatus and is necessary for legacy uses.
        String defaultstatus() const { return defaultStatus(); }
        void setDefaultstatus(const String& status) { setDefaultStatus(status); }

        // Self referential attributes
        DOMWindow* self() const;
        DOMWindow* window() const { return self(); }
        DOMWindow* frames() const { return self(); }

        DOMWindow* opener() const;
        DOMWindow* parent() const;
        DOMWindow* top() const;

        // DOM Level 2 AbstractView Interface
        Document* document() const;

        // DOM Level 2 Style Interface
        PassRefPtr<CSSStyleDeclaration> getComputedStyle(Element*, const String& pseudoElt) const;

        // WebKit extensions
        PassRefPtr<CSSRuleList> getMatchedCSSRules(Element*, const String& pseudoElt, bool authorOnly = true) const;
        double devicePixelRatio() const;

#if ENABLE(DATABASE)
        // HTML 5 client-side database
        PassRefPtr<Database> openDatabase(const String& name, const String& version, const String& displayName, unsigned long estimatedSize, ExceptionCode&);
#endif

        Console* console() const;
        
#if ENABLE(CROSS_DOCUMENT_MESSAGING)
        void postMessage(const String& message, const String& domain, const String& uri, DOMWindow* source) const;
#endif

        void scrollBy(int x, int y) const;
        void scrollTo(int x, int y) const;
        void scroll(int x, int y) const { scrollTo(x, y); }

        void moveBy(float x, float y) const;
        void moveTo(float x, float y) const;

        void resizeBy(float x, float y) const;
        void resizeTo(float width, float height) const;

    private:
        Frame* m_frame;
        mutable RefPtr<Screen> m_screen;
        mutable RefPtr<DOMSelection> m_selection;
        mutable RefPtr<History> m_history;
        mutable RefPtr<BarInfo> m_locationbar;
        mutable RefPtr<BarInfo> m_menubar;
        mutable RefPtr<BarInfo> m_personalbar;
        mutable RefPtr<BarInfo> m_scrollbars;
        mutable RefPtr<BarInfo> m_statusbar;
        mutable RefPtr<BarInfo> m_toolbar;
        mutable RefPtr<Console> m_console;
    };

} // namespace WebCore

#endif
