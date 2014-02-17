/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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

#ifndef HTMLFrameOwnerElement_h
#define HTMLFrameOwnerElement_h

#include "HTMLElement.h"
#include <wtf/HashCountedSet.h>

namespace WebCore {

class DOMWindow;
class Frame;
class RenderWidget;
class SVGDocument;

class HTMLFrameOwnerElement : public HTMLElement {
public:
    virtual ~HTMLFrameOwnerElement();

    Frame* contentFrame() const { return m_contentFrame; }
    DOMWindow* contentWindow() const;
    Document* contentDocument() const;

    void setContentFrame(Frame*);
    void clearContentFrame();

    void disconnectContentFrame();

    // Most subclasses use RenderWidget (either RenderEmbeddedObject or RenderIFrame)
    // except for HTMLObjectElement and HTMLEmbedElement which may return any
    // RenderElement when using fallback content.
    RenderWidget* renderWidget() const;

    SVGDocument* getSVGDocument(ExceptionCode&) const;

    virtual ScrollbarMode scrollingMode() const { return ScrollbarAuto; }

    SandboxFlags sandboxFlags() const { return m_sandboxFlags; }

    void scheduleSetNeedsStyleRecalc(StyleChangeType = FullStyleChange);

protected:
    HTMLFrameOwnerElement(const QualifiedName& tagName, Document&);
    void setSandboxFlags(SandboxFlags);

private:
    virtual bool isKeyboardFocusable(KeyboardEvent*) const override;
    virtual bool isFrameOwnerElement() const override { return true; }

    Frame* m_contentFrame;
    SandboxFlags m_sandboxFlags;
};

void isHTMLFrameOwnerElement(const HTMLFrameOwnerElement&); // Catch unnecessary runtime check of type known at compile time.
inline bool isHTMLFrameOwnerElement(const Node& node) { return node.isFrameOwnerElement(); }
NODE_TYPE_CASTS(HTMLFrameOwnerElement)

class SubframeLoadingDisabler {
public:
    explicit SubframeLoadingDisabler(ContainerNode& root)
        : m_root(root)
    {
        disabledSubtreeRoots().add(&m_root);
    }

    ~SubframeLoadingDisabler()
    {
        disabledSubtreeRoots().remove(&m_root);
    }

    static bool canLoadFrame(HTMLFrameOwnerElement&);

private:
    static HashCountedSet<ContainerNode*>& disabledSubtreeRoots()
    {
        DEFINE_STATIC_LOCAL(HashCountedSet<ContainerNode*>, nodes, ());
        return nodes;
    }

    ContainerNode& m_root;
};

} // namespace WebCore

#endif // HTMLFrameOwnerElement_h
