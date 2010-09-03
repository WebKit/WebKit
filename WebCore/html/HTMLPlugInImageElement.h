/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLPlugInImageElement_h
#define HTMLPlugInImageElement_h

#include "HTMLPlugInElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLImageLoader;
class FrameLoader;

// Base class for HTMLObjectElement and HTMLEmbedElement
class HTMLPlugInImageElement : public HTMLPlugInElement {
public:
    RenderEmbeddedObject* renderEmbeddedObject() const;

    virtual void updateWidget(bool onlyCreateNonNetscapePlugins) = 0;

protected:
    HTMLPlugInImageElement(const QualifiedName& tagName, Document*, bool createdByParser);

    bool isImageType();

    const String& serviceType() const { return m_serviceType; }
    const String& url() const { return m_url; }

    OwnPtr<HTMLImageLoader> m_imageLoader;
    String m_serviceType;
    String m_url;
    
    static void updateWidgetCallback(Node*);
    virtual void attach();
    virtual void detach();

    bool needsWidgetUpdate() const { return m_needsWidgetUpdate; }
    void setNeedsWidgetUpdate(bool needsWidgetUpdate) { m_needsWidgetUpdate = needsWidgetUpdate; }

    bool allowedToLoadFrameURL(const String& url);
    bool wouldLoadAsNetscapePlugin(const String& url, const String& serviceType);

private:
    virtual bool canLazyAttach() { return false; }
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void recalcStyle(StyleChange);
    
    virtual void finishParsingChildren();
    virtual void willMoveToNewOwnerDocument();

    void updateWidgetIfNecessary();
    virtual bool useFallbackContent() const { return false; }
    
    bool m_needsWidgetUpdate;
};

} // namespace WebCore

#endif // HTMLPlugInImageElement_h
