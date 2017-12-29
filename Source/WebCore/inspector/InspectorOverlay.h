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
#include "LayoutRect.h"
#include "NodeList.h"
#include "Timer.h"
#include <inspector/InspectorProtocolObjects.h>
#include <wtf/Deque.h>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Color;
class GraphicsContext;
class InspectorClient;
class IntRect;
class Node;
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

    void setPausedInDebuggerMessage(const String*);

    void hideHighlight();
    void highlightNodeList(RefPtr<NodeList>&&, const HighlightConfig&);
    void highlightNode(Node*, const HighlightConfig&);
    void highlightQuad(std::unique_ptr<FloatQuad>, const HighlightConfig&);
    
    void setShowingPaintRects(bool);
    void showPaintRect(const FloatRect&);
    
    Node* highlightedNode() const;

    void didSetSearchingForNode(bool enabled);

    void setIndicating(bool indicating);

    RefPtr<Inspector::Protocol::OverlayTypes::NodeHighlightData> buildHighlightObjectForNode(Node*, HighlightType) const;
    Ref<JSON::ArrayOf<Inspector::Protocol::OverlayTypes::NodeHighlightData>> buildObjectForHighlightedNodes() const;

    void freePage();
private:
    bool shouldShowOverlay() const;
    void drawGutter();
    void drawNodeHighlight();
    void drawQuadHighlight();
    void drawPausedInDebuggerMessage();
    void drawPaintRects();
    void updatePaintRectsTimerFired();

    Page* overlayPage();

    void forcePaint();
    void reset(const IntSize& viewportSize, const IntSize& frameViewFullSize);
    void evaluateInOverlay(const String& method);
    void evaluateInOverlay(const String& method, const String& argument);
    void evaluateInOverlay(const String& method, RefPtr<JSON::Value>&& argument);

    Page& m_page;
    InspectorClient* m_client;
    String m_pausedInDebuggerMessage;
    RefPtr<Node> m_highlightNode;
    RefPtr<NodeList> m_highlightNodeList;
    HighlightConfig m_nodeHighlightConfig;
    std::unique_ptr<FloatQuad> m_highlightQuad;
    std::unique_ptr<Page> m_overlayPage;
    HighlightConfig m_quadHighlightConfig;
    
    using TimeRectPair = std::pair<MonotonicTime, FloatRect>;
    Deque<TimeRectPair> m_paintRects;
    Timer m_paintRectUpdateTimer;
    bool m_indicating {false};
    bool m_showingPaintRects {false};
};

} // namespace WebCore
