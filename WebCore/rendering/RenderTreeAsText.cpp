/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RenderTreeAsText.h"

#include "CharacterNames.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "RenderBR.h"
#include "RenderListMarker.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SelectionController.h"
#include <wtf/Vector.h>

#if ENABLE(SVG)
#include "RenderSVGRoot.h"
#include "RenderSVGContainer.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGText.h"
#include "SVGRenderTreeAsText.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static void writeLayers(TextStream&, const RenderLayer* rootLayer, RenderLayer*, const IntRect& paintDirtyRect, int indent = 0);

#if !ENABLE(SVG)
static TextStream &operator<<(TextStream& ts, const IntRect& r)
{
    return ts << "at (" << r.x() << "," << r.y() << ") size " << r.width() << "x" << r.height();
}
#endif

static void writeIndent(TextStream& ts, int indent)
{
    for (int i = 0; i != indent; ++i)
        ts << "  ";
}

static void printBorderStyle(TextStream& ts, const RenderObject& o, const EBorderStyle borderStyle)
{
    switch (borderStyle) {
        case BNONE:
            ts << "none";
            break;
        case BHIDDEN:
            ts << "hidden";
            break;
        case INSET:
            ts << "inset";
            break;
        case GROOVE:
            ts << "groove";
            break;
        case RIDGE:
            ts << "ridge";
            break;
        case OUTSET:
            ts << "outset";
            break;
        case DOTTED:
            ts << "dotted";
            break;
        case DASHED:
            ts << "dashed";
            break;
        case SOLID:
            ts << "solid";
            break;
        case DOUBLE:
            ts << "double";
            break;
    }

    ts << " ";
}

static String getTagName(Node* n)
{
    if (n->isDocumentNode())
        return "";
    if (n->isCommentNode())
        return "COMMENT";
    return n->nodeName();
}

static bool isEmptyOrUnstyledAppleStyleSpan(const Node* node)
{
    if (!node || !node->isHTMLElement() || !node->hasTagName(spanTag))
        return false;

    const HTMLElement* elem = static_cast<const HTMLElement*>(node);
    if (elem->getAttribute(classAttr) != "Apple-style-span")
        return false;

    if (!node->hasChildNodes())
        return true;

    CSSMutableStyleDeclaration* inlineStyleDecl = elem->inlineStyleDecl();
    return (!inlineStyleDecl || inlineStyleDecl->length() == 0);
}

String quoteAndEscapeNonPrintables(const String& s)
{
    Vector<UChar> result;
    result.append('"');
    for (unsigned i = 0; i != s.length(); ++i) {
        UChar c = s[i];
        if (c == '\\') {
            result.append('\\');
            result.append('\\');
        } else if (c == '"') {
            result.append('\\');
            result.append('"');
        } else if (c == '\n' || c == noBreakSpace)
            result.append(' ');
        else {
            if (c >= 0x20 && c < 0x7F)
                result.append(c);
            else {
                unsigned u = c;
                String hex = String::format("\\x{%X}", u);
                unsigned len = hex.length();
                for (unsigned i = 0; i < len; ++i)
                    result.append(hex[i]);
            }
        }
    }
    result.append('"');
    return String::adopt(result);
}

static TextStream &operator<<(TextStream& ts, const RenderObject& o)
{
    ts << o.renderName();

    if (o.style() && o.style()->zIndex())
        ts << " zI: " << o.style()->zIndex();

    if (o.element()) {
        String tagName = getTagName(o.element());
        if (!tagName.isEmpty()) {
            ts << " {" << tagName << "}";
            // flag empty or unstyled AppleStyleSpan because we never
            // want to leave them in the DOM
            if (isEmptyOrUnstyledAppleStyleSpan(o.element()))
                ts << " *empty or unstyled AppleStyleSpan*";
        }
    }

    IntRect r(o.xPos(), o.yPos(), o.width(), o.height());
    ts << " " << r;

    if (!(o.isText() && !o.isBR())) {
        if (o.parent() && (o.parent()->style()->color() != o.style()->color()))
            ts << " [color=" << o.style()->color().name() << "]";

        if (o.parent() && (o.parent()->style()->backgroundColor() != o.style()->backgroundColor()) &&
            o.style()->backgroundColor().isValid() && o.style()->backgroundColor().rgb())
            // Do not dump invalid or transparent backgrounds, since that is the default.
            ts << " [bgcolor=" << o.style()->backgroundColor().name() << "]";
        
        if (o.parent() && (o.parent()->style()->textFillColor() != o.style()->textFillColor()) &&
            o.style()->textFillColor().isValid() && o.style()->textFillColor() != o.style()->color() &&
            o.style()->textFillColor().rgb())
            ts << " [textFillColor=" << o.style()->textFillColor().name() << "]";

        if (o.parent() && (o.parent()->style()->textStrokeColor() != o.style()->textStrokeColor()) &&
            o.style()->textStrokeColor().isValid() && o.style()->textStrokeColor() != o.style()->color() &&
            o.style()->textStrokeColor().rgb())
            ts << " [textStrokeColor=" << o.style()->textStrokeColor().name() << "]";

        if (o.parent() && (o.parent()->style()->textStrokeWidth() != o.style()->textStrokeWidth()) &&
            o.style()->textStrokeWidth() > 0)
            ts << " [textStrokeWidth=" << o.style()->textStrokeWidth() << "]";

        if (o.borderTop() || o.borderRight() || o.borderBottom() || o.borderLeft()) {
            ts << " [border:";

            BorderValue prevBorder;
            if (o.style()->borderTop() != prevBorder) {
                prevBorder = o.style()->borderTop();
                if (!o.borderTop())
                    ts << " none";
                else {
                    ts << " (" << o.borderTop() << "px ";
                    printBorderStyle(ts, o, o.style()->borderTopStyle());
                    Color col = o.style()->borderTopColor();
                    if (!col.isValid())
                        col = o.style()->color();
                    ts << col.name() << ")";
                }
            }

            if (o.style()->borderRight() != prevBorder) {
                prevBorder = o.style()->borderRight();
                if (!o.borderRight())
                    ts << " none";
                else {
                    ts << " (" << o.borderRight() << "px ";
                    printBorderStyle(ts, o, o.style()->borderRightStyle());
                    Color col = o.style()->borderRightColor();
                    if (!col.isValid())
                        col = o.style()->color();
                    ts << col.name() << ")";
                }
            }

            if (o.style()->borderBottom() != prevBorder) {
                prevBorder = o.style()->borderBottom();
                if (!o.borderBottom())
                    ts << " none";
                else {
                    ts << " (" << o.borderBottom() << "px ";
                    printBorderStyle(ts, o, o.style()->borderBottomStyle());
                    Color col = o.style()->borderBottomColor();
                    if (!col.isValid())
                        col = o.style()->color();
                    ts << col.name() << ")";
                }
            }

            if (o.style()->borderLeft() != prevBorder) {
                prevBorder = o.style()->borderLeft();
                if (!o.borderLeft())
                    ts << " none";
                else {
                    ts << " (" << o.borderLeft() << "px ";
                    printBorderStyle(ts, o, o.style()->borderLeftStyle());
                    Color col = o.style()->borderLeftColor();
                    if (!col.isValid())
                        col = o.style()->color();
                    ts << col.name() << ")";
                }
            }

            ts << "]";
        }
    }

    if (o.isTableCell()) {
        const RenderTableCell& c = static_cast<const RenderTableCell&>(o);
        ts << " [r=" << c.row() << " c=" << c.col() << " rs=" << c.rowSpan() << " cs=" << c.colSpan() << "]";
    }

    if (o.isListMarker()) {
        String text = static_cast<const RenderListMarker&>(o).text();
        if (!text.isEmpty()) {
            if (text.length() != 1)
                text = quoteAndEscapeNonPrintables(text);
            else {
                switch (text[0]) {
                    case bullet:
                        text = "bullet";
                        break;
                    case blackSquare:
                        text = "black square";
                        break;
                    case whiteBullet:
                        text = "white bullet";
                        break;
                    default:
                        text = quoteAndEscapeNonPrintables(text);
                }
            }
            ts << ": " << text;
        }
    }

    return ts;
}

static void writeTextRun(TextStream& ts, const RenderText& o, const InlineTextBox& run)
{
    ts << "text run at (" << run.m_x << "," << run.m_y << ") width " << run.m_width;
    if (run.m_reversed || run.m_dirOverride) {
        ts << (run.m_reversed ? " RTL" : " LTR");
        if (run.m_dirOverride)
            ts << " override";
    }
    ts << ": "
        << quoteAndEscapeNonPrintables(String(o.text()).substring(run.m_start, run.m_len))
        << "\n";
}

void write(TextStream& ts, const RenderObject& o, int indent)
{
#if ENABLE(SVG)
    if (o.isRenderPath()) {
        write(ts, static_cast<const RenderPath&>(o), indent);
        return;
    }
    if (o.isSVGContainer()) {
        write(ts, static_cast<const RenderSVGContainer&>(o), indent);
        return;
    }
    if (o.isSVGRoot()) {
        write(ts, static_cast<const RenderSVGRoot&>(o), indent);
        return;
    }
    if (o.isSVGText()) {
        if (!o.isText())
            write(ts, static_cast<const RenderSVGText&>(o), indent);
        else
            write(ts, static_cast<const RenderSVGInlineText&>(o), indent);
        return;
    }
#endif

    writeIndent(ts, indent);

    ts << o << "\n";

    if (o.isText() && !o.isBR()) {
        const RenderText& text = static_cast<const RenderText&>(o);
        for (InlineTextBox* box = text.firstTextBox(); box; box = box->nextTextBox()) {
            writeIndent(ts, indent + 1);
            writeTextRun(ts, text, *box);
        }
    }

    for (RenderObject* child = o.firstChild(); child; child = child->nextSibling()) {
        if (child->hasLayer())
            continue;
        write(ts, *child, indent + 1);
    }

    if (o.isWidget()) {
        Widget* widget = static_cast<const RenderWidget&>(o).widget();
        if (widget && widget->isFrameView()) {
            FrameView* view = static_cast<FrameView*>(widget);
            RenderObject* root = view->frame()->renderer();
            if (root) {
                view->layout();
                RenderLayer* l = root->layer();
                if (l)
                    writeLayers(ts, l, l, IntRect(l->xPos(), l->yPos(), l->width(), l->height()), indent + 1);
            }
        }
    }
}

static void write(TextStream& ts, RenderLayer& l,
                  const IntRect& layerBounds, const IntRect& backgroundClipRect, const IntRect& clipRect, const IntRect& outlineClipRect,
                  int layerType = 0, int indent = 0)
{
    writeIndent(ts, indent);

    ts << "layer " << layerBounds;

    if (!layerBounds.isEmpty()) {
        if (!backgroundClipRect.contains(layerBounds))
            ts << " backgroundClip " << backgroundClipRect;
        if (!clipRect.contains(layerBounds))
            ts << " clip " << clipRect;
        if (!outlineClipRect.contains(layerBounds))
            ts << " outlineClip " << outlineClipRect;
    }

    if (l.renderer()->hasOverflowClip()) {
        if (l.scrollXOffset())
            ts << " scrollX " << l.scrollXOffset();
        if (l.scrollYOffset())
            ts << " scrollY " << l.scrollYOffset();
        if (l.renderer()->clientWidth() != l.scrollWidth())
            ts << " scrollWidth " << l.scrollWidth();
        if (l.renderer()->clientHeight() != l.scrollHeight())
            ts << " scrollHeight " << l.scrollHeight();
    }

    if (layerType == -1)
        ts << " layerType: background only";
    else if (layerType == 1)
        ts << " layerType: foreground only";

    ts << "\n";

    if (layerType != -1)
        write(ts, *l.renderer(), indent + 1);
}

static void writeLayers(TextStream& ts, const RenderLayer* rootLayer, RenderLayer* l,
                        const IntRect& paintDirtyRect, int indent)
{
    // Calculate the clip rects we should use.
    IntRect layerBounds, damageRect, clipRectToApply, outlineRect;
    l->calculateRects(rootLayer, paintDirtyRect, layerBounds, damageRect, clipRectToApply, outlineRect);

    // Ensure our lists are up-to-date.
    l->updateZOrderLists();
    l->updateOverflowList();

    bool shouldPaint = l->intersectsDamageRect(layerBounds, damageRect, rootLayer);
    Vector<RenderLayer*>* negList = l->negZOrderList();
    if (shouldPaint && negList && negList->size() > 0)
        write(ts, *l, layerBounds, damageRect, clipRectToApply, outlineRect, -1, indent);

    if (negList) {
        for (unsigned i = 0; i != negList->size(); ++i)
            writeLayers(ts, rootLayer, negList->at(i), paintDirtyRect, indent);
    }

    if (shouldPaint)
        write(ts, *l, layerBounds, damageRect, clipRectToApply, outlineRect, negList && negList->size() > 0, indent);

    Vector<RenderLayer*>* overflowList = l->overflowList();
    if (overflowList) {
        for (unsigned i = 0; i != overflowList->size(); ++i)
            writeLayers(ts, rootLayer, overflowList->at(i), paintDirtyRect, indent);
    }

    Vector<RenderLayer*>* posList = l->posZOrderList();
    if (posList) {
        for (unsigned i = 0; i != posList->size(); ++i)
            writeLayers(ts, rootLayer, posList->at(i), paintDirtyRect, indent);
    }
}

static String nodePosition(Node* node)
{
    String result;

    Node* parent;
    for (Node* n = node; n; n = parent) {
        parent = n->parentNode();
        if (!parent)
            parent = n->shadowParentNode();
        if (n != node)
            result += " of ";
        if (parent)
            result += "child " + String::number(n->nodeIndex()) + " {" + getTagName(n) + "}";
        else
            result += "document";
    }

    return result;
}

static void writeSelection(TextStream& ts, const RenderObject* o)
{
    Node* n = o->element();
    if (!n || !n->isDocumentNode())
        return;

    Document* doc = static_cast<Document*>(n);
    Frame* frame = doc->frame();
    if (!frame)
        return;

    Selection selection = frame->selectionController()->selection();
    if (selection.isCaret()) {
        ts << "caret: position " << selection.start().offset() << " of " << nodePosition(selection.start().node());
        if (selection.affinity() == UPSTREAM)
            ts << " (upstream affinity)";
        ts << "\n";
    } else if (selection.isRange())
        ts << "selection start: position " << selection.start().offset() << " of " << nodePosition(selection.start().node()) << "\n"
           << "selection end:   position " << selection.end().offset() << " of " << nodePosition(selection.end().node()) << "\n";
}

DeprecatedString externalRepresentation(RenderObject* o)
{
    DeprecatedString s;
    if (o) {
        TextStream ts(&s);
        ts.precision(2);
#if ENABLE(SVG)
        writeRenderResources(ts, o->document());
#endif
        if (o->view()->frameView())
            o->view()->frameView()->layout();
        RenderLayer* l = o->layer();
        if (l) {
            writeLayers(ts, l, l, IntRect(l->xPos(), l->yPos(), l->width(), l->height()));
            writeSelection(ts, o);
        }
    }
    return s;
}

} // namespace WebCore
