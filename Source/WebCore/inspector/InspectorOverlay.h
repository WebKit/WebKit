/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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

#pragma once

#include "Color.h"
#include "FloatQuad.h"
#include "FloatRect.h"
#include "Path.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GraphicsContext;
class InspectorClient;
class Node;
class NodeList;
class Page;

struct HighlightConfig {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Color content;
    Color contentOutline;
    Color padding;
    Color border;
    Color margin;
    bool showInfo;
    bool usePageCoordinates;
};

enum class HighlightType {
    Node, // Provides 4 quads: margin, border, padding, content.
    NodeList, // Provides a list of nodes.
    Rects, // Provides a list of quads.
};

struct Highlight {
    Highlight() { }

    void setDataFromConfig(const HighlightConfig& highlightConfig)
    {
        contentColor = highlightConfig.content;
        contentOutlineColor = highlightConfig.contentOutline;
        paddingColor = highlightConfig.padding;
        borderColor = highlightConfig.border;
        marginColor = highlightConfig.margin;
        usePageCoordinates = highlightConfig.usePageCoordinates;
    }

    Color contentColor;
    Color contentOutlineColor;
    Color paddingColor;
    Color borderColor;
    Color marginColor;

    HighlightType type {HighlightType::Node};
    Vector<FloatQuad> quads;
    bool usePageCoordinates {true};

    using Bounds = FloatRect;
};

class InspectorOverlay {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorOverlay(Page&, InspectorClient*);
    ~InspectorOverlay();

    enum class CoordinateSystem {
        View, // Adjusts for the main frame's scroll offset.
        Document, // Does not adjust for the main frame's scroll offset.
    };

    void update();
    void paint(GraphicsContext&);
    void getHighlight(Highlight&, CoordinateSystem) const;
    bool shouldShowOverlay() const;

    void hideHighlight();
    void highlightNodeList(RefPtr<NodeList>&&, const HighlightConfig&);
    void highlightNode(Node*, const HighlightConfig&);
    void highlightQuad(std::unique_ptr<FloatQuad>, const HighlightConfig&);

    void setShowPaintRects(bool);
    void showPaintRect(const FloatRect&);

    void setShowRulers(bool);
    void setShowRulersDuringElementSelection(bool enabled) { m_showRulersDuringElementSelection = enabled; }

    Node* highlightedNode() const;

    void didSetSearchingForNode(bool enabled);

    void setIndicating(bool indicating);

private:
    using TimeRectPair = std::pair<MonotonicTime, FloatRect>;

    struct RulerExclusion {
        Highlight::Bounds bounds;
        Path titlePath;
    };

    RulerExclusion drawNodeHighlight(GraphicsContext&, Node&);
    RulerExclusion drawQuadHighlight(GraphicsContext&, const FloatQuad&);
    void drawPaintRects(GraphicsContext&, const Deque<TimeRectPair>&);
    void drawBounds(GraphicsContext&, const Highlight::Bounds&);
    void drawRulers(GraphicsContext&, const RulerExclusion&);

    Path drawElementTitle(GraphicsContext&, Node&, const Highlight::Bounds&);

    void updatePaintRectsTimerFired();

    Page& m_page;
    InspectorClient* m_client;

    RefPtr<Node> m_highlightNode;
    RefPtr<NodeList> m_highlightNodeList;
    HighlightConfig m_nodeHighlightConfig;

    std::unique_ptr<FloatQuad> m_highlightQuad;
    HighlightConfig m_quadHighlightConfig;

    Deque<TimeRectPair> m_paintRects;
    Timer m_paintRectUpdateTimer;

    bool m_indicating { false };
    bool m_showPaintRects { false };
    bool m_showRulers { false };
    bool m_showRulersDuringElementSelection { false };
};

} // namespace WebCore
