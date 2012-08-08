/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorOverlay_h
#define InspectorOverlay_h

#include "Color.h"
#include "FloatQuad.h"
#include "LayoutTypes.h"
#include "Node.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class Color;
class GraphicsContext;
class InspectorClient;
class IntRect;
class Node;
class Page;

struct HighlightConfig {
    Color content;
    Color contentOutline;
    Color padding;
    Color border;
    Color margin;
    bool showInfo;
};

enum HighlightType {
    HighlightTypeNode,
    HighlightTypeRects,
};

struct Highlight {
    void setColors(const HighlightConfig& highlightConfig)
    {
        contentColor = highlightConfig.content;
        paddingColor = highlightConfig.padding;
        borderColor = highlightConfig.border;
        marginColor = highlightConfig.margin;
    }

    Color contentColor;
    Color paddingColor;
    Color borderColor;
    Color marginColor;

    // When the type is Node, there are 4 quads (margin, border, padding, content).
    // When the type is Rects, this is just a list of quads.
    HighlightType type;
    Vector<FloatQuad> quads;
};

class InspectorOverlay {
public:
    static PassOwnPtr<InspectorOverlay> create(Page* page, InspectorClient* client)
    {
        return adoptPtr(new InspectorOverlay(page, client));
    }

    void paint(GraphicsContext&);
    void drawOutline(GraphicsContext&, const LayoutRect&, const Color&);
    void getHighlight(Highlight*) const;

    void setPausedInDebuggerMessage(const String*);

    void hideHighlight();
    void highlightNode(Node*, const HighlightConfig&);
    void highlightRect(PassOwnPtr<IntRect>, const HighlightConfig&);

    Node* highlightedNode() const;

private:
    InspectorOverlay(Page*, InspectorClient*);

    void update();
    void drawNodeHighlight(GraphicsContext&);
    void drawRectHighlight(GraphicsContext&);
    void drawPausedInDebugger(GraphicsContext&);

    Page* m_page;
    InspectorClient* m_client;
    String m_pausedInDebuggerMessage;
    RefPtr<Node> m_highlightNode;
    HighlightConfig m_nodeHighlightConfig;
    OwnPtr<IntRect> m_highlightRect;
    HighlightConfig m_rectHighlightConfig;
};

} // namespace WebCore


#endif // InspectorOverlay_h
