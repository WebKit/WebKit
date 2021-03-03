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
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace Inspector {
using ErrorString = String;

template <typename T>
using ErrorStringOr = Expected<T, ErrorString>;
}

namespace WebCore {

class FloatPoint;
class GraphicsContext;
class InspectorClient;
class Node;
class NodeList;
class Page;

class InspectorOverlay {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorOverlay(Page&, InspectorClient*);
    ~InspectorOverlay();

    struct Highlight {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        enum class Type {
            Node, // Provides 4 quads: margin, border, padding, content.
            NodeList, // Provides a list of nodes.
            Rects, // Provides a list of quads.
        };

        struct Config {
            WTF_MAKE_STRUCT_FAST_ALLOCATED;
            Color content;
            Color contentOutline;
            Color padding;
            Color border;
            Color margin;
            bool showInfo;
            bool usePageCoordinates;
        };

        void setDataFromConfig(const Config& config)
        {
            contentColor = config.content;
            contentOutlineColor = config.contentOutline;
            paddingColor = config.padding;
            borderColor = config.border;
            marginColor = config.margin;
            usePageCoordinates = config.usePageCoordinates;
        }

        Color contentColor;
        Color contentOutlineColor;
        Color paddingColor;
        Color borderColor;
        Color marginColor;

        Type type {Type::Node};
        Vector<FloatQuad> quads;
        bool usePageCoordinates {true};

        using Bounds = FloatRect;
    };

    struct Grid {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        struct Config {
            WTF_MAKE_STRUCT_FAST_ALLOCATED;

            Color gridColor;
            bool showLineNames;
            bool showLineNumbers;
            bool showExtendedGridLines;
            bool showTrackSizes;
            bool showAreaNames;
        };

        WeakPtr<Node> gridNode;
        Config config;
    };

    enum class CoordinateSystem {
        View, // Adjusts for the main frame's scroll offset.
        Document, // Does not adjust for the main frame's scroll offset.
    };

    void update();
    void paint(GraphicsContext&);
    void getHighlight(Highlight&, CoordinateSystem) const;
    bool shouldShowOverlay() const;

    void hideHighlight();
    void highlightNodeList(RefPtr<NodeList>&&, const Highlight::Config&);
    void highlightNode(Node*, const Highlight::Config&);
    void highlightQuad(std::unique_ptr<FloatQuad>, const Highlight::Config&);

    void setShowPaintRects(bool);
    void showPaintRect(const FloatRect&);

    void setShowRulers(bool);
    void setShowRulersDuringElementSelection(bool enabled) { m_showRulersDuringElementSelection = enabled; }

    Node* highlightedNode() const;
    unsigned gridOverlayCount() const { return m_activeGridOverlays.size(); }

    void didSetSearchingForNode(bool enabled);

    void setIndicating(bool indicating);

    // Multiple grid overlays can be active at the same time. These methods
    // will fail if the node is not a grid or if the node has been GC'd.
    Inspector::ErrorStringOr<void> setGridOverlayForNode(Node&, const InspectorOverlay::Grid::Config&);
    Inspector::ErrorStringOr<void> clearGridOverlayForNode(Node&);
    void clearAllGridOverlays();

private:
    using TimeRectPair = std::pair<MonotonicTime, FloatRect>;

    struct RulerExclusion {
        Highlight::Bounds bounds;
        Path titlePath;
    };
    
    enum class LabelArrowDirection {
        None,
        Down,
        Up,
        Left,
        Right,
    };

    RulerExclusion drawNodeHighlight(GraphicsContext&, Node&);
    RulerExclusion drawQuadHighlight(GraphicsContext&, const FloatQuad&);
    void drawPaintRects(GraphicsContext&, const Deque<TimeRectPair>&);
    void drawBounds(GraphicsContext&, const Highlight::Bounds&);
    void drawRulers(GraphicsContext&, const RulerExclusion&);

    Path drawElementTitle(GraphicsContext&, Node&, const Highlight::Bounds&);
    
    void drawLayoutHatching(GraphicsContext&, FloatRect, IntPoint);
    void drawLayoutLabel(GraphicsContext&, String, FloatPoint, LabelArrowDirection, Color backgroundColor = Color::white, float maximumWidth = 0);

    void drawGridOverlay(GraphicsContext&, const InspectorOverlay::Grid&);

    void updatePaintRectsTimerFired();

    bool removeGridOverlayForNode(Node&);

    Page& m_page;
    InspectorClient* m_client;

    RefPtr<Node> m_highlightNode;
    RefPtr<NodeList> m_highlightNodeList;
    Highlight::Config m_nodeHighlightConfig;

    std::unique_ptr<FloatQuad> m_highlightQuad;
    Highlight::Config m_quadHighlightConfig;

    Deque<TimeRectPair> m_paintRects;
    Timer m_paintRectUpdateTimer;

    Vector<InspectorOverlay::Grid> m_activeGridOverlays;

    bool m_indicating { false };
    bool m_showPaintRects { false };
    bool m_showRulers { false };
    bool m_showRulersDuringElementSelection { false };
};

} // namespace WebCore
