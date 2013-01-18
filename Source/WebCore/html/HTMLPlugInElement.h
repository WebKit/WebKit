/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLPlugInElement_h
#define HTMLPlugInElement_h

#include "HTMLFrameOwnerElement.h"
#include "Image.h"
#include "ImageLoaderClient.h"

#include "ScriptInstance.h"

#if ENABLE(NETSCAPE_PLUGIN_API)
struct NPObject;
#endif

namespace WebCore {

class RenderEmbeddedObject;
class RenderWidget;
class Widget;

class HTMLPlugInElement : public HTMLFrameOwnerElement {
public:
    virtual ~HTMLPlugInElement();

    void resetInstance();

    PassScriptInstance getInstance();

    Widget* pluginWidget() const;

    enum DisplayState {
        WaitingForSnapshot,
        DisplayingSnapshot,
        PlayingWithPendingMouseClick,
        Playing
    };
    DisplayState displayState() const { return m_displayState; }
    void setDisplayState(DisplayState state) { m_displayState = state; }
    virtual void updateSnapshot(PassRefPtr<Image>) { }
    virtual void dispatchPendingMouseClick() { }

    unsigned plugInOriginHash() const { return m_plugInOriginHash; }

#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* getNPObject();
#endif

    bool isCapturingMouseEvents() const { return m_isCapturingMouseEvents; }
    void setIsCapturingMouseEvents(bool capturing) { m_isCapturingMouseEvents = capturing; }

    bool canContainRangeEndPoint() const { return false; }

    bool canProcessDrag() const;

    virtual bool willRespondToMouseClickEvents() OVERRIDE;

protected:
    HTMLPlugInElement(const QualifiedName& tagName, Document*);

    virtual void detach();
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const Attribute&, StylePropertySet*) OVERRIDE;
    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }

    // Subclasses should use guardedDispatchBeforeLoadEvent instead of calling dispatchBeforeLoadEvent directly.
    bool guardedDispatchBeforeLoadEvent(const String& sourceURL);

    bool m_inBeforeLoadEventHandler;

    unsigned m_plugInOriginHash;

private:
    bool dispatchBeforeLoadEvent(const String& sourceURL); // Not implemented, generates a compile error if subclasses call this by mistake.

    virtual void defaultEventHandler(Event*);

    virtual RenderWidget* renderWidgetForJSBindings() const = 0;

    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isPluginElement() const;

private:
    mutable ScriptInstance m_instance;
#if ENABLE(NETSCAPE_PLUGIN_API)
    NPObject* m_NPObject;
#endif
    bool m_isCapturingMouseEvents;

    DisplayState m_displayState;
};

} // namespace WebCore

#endif // HTMLPlugInElement_h
