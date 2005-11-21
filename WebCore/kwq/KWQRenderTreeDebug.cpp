/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#include "KWQRenderTreeDebug.h"

#include "dom_docimpl.h"
#include "dom_position.h"
#include "jsediting.h"
#include "khtmlview.h"
#include "render_canvas.h"
#include "render_replaced.h"
#include "render_table.h"
#include "render_text.h"
#include "render_br.h"
#include "SelectionController.h"

#if SVG_SUPPORT
#include "KCanvasTreeDebug.h"
#include "KCanvasItem.h"
#include "KCanvasContainer.h"
#endif

#include "KWQKHTMLPart.h"
#include "KWQTextStream.h"

using DOM::DocumentImpl;
using DOM::JSEditor;
using DOM::NodeImpl;
using DOM::Position;

using khtml::BorderValue;
using khtml::EBorderStyle;
using khtml::InlineTextBox;
using khtml::RenderLayer;
using khtml::RenderObject;
using khtml::RenderTableCell;
using khtml::RenderWidget;
using khtml::RenderText;
using khtml::RenderCanvas;
using khtml::RenderBR;
using khtml::SelectionController;
using khtml::transparentColor;
using khtml::UPSTREAM;

static void writeLayers(QTextStream &ts, const RenderLayer* rootLayer, RenderLayer* l,
                        const QRect& paintDirtyRect, int indent=0);

#if !SVG_SUPPORT
static QTextStream &operator<<(QTextStream &ts, const QRect &r)
{
    return ts << "at (" << r.x() << "," << r.y() << ") size " << r.width() << "x" << r.height();
}
#endif

static void writeIndent(QTextStream &ts, int indent)
{
    for (int i = 0; i != indent; ++i) {
        ts << "  ";
    }
}

static void printBorderStyle(QTextStream &ts, const RenderObject &o, const EBorderStyle borderStyle)
{
    switch (borderStyle) {
        case khtml::BNONE:
            ts << "none";
            break;
        case khtml::BHIDDEN:
            ts << "hidden";
            break;
        case khtml::INSET:
            ts << "inset";
            break;
        case khtml::GROOVE:
            ts << "groove";
            break;
        case khtml::RIDGE:
            ts << "ridge";
            break;
        case khtml::OUTSET:
            ts << "outset";
            break;
        case khtml::DOTTED:
            ts << "dotted";
            break;
        case khtml::DASHED:
            ts << "dashed";
            break;
        case khtml::SOLID:
            ts << "solid";
            break;
        case khtml::DOUBLE:
            ts << "double";
            break;
    }
    
    ts << " ";
}

static QString getTagName(NodeImpl *n)
{
    if (n->isDocumentNode())
        return "";
    if (n->isTextNode())
        return "TEXT"; // FIXME: Remove once the layout tests are ready to change.
    if (n->isCommentNode())
        return "COMMENT";
    if (n->isHTMLElement())
        return n->nodeName().upper().qstring(); // FIXME: We want to dump the real DOM name, not an uppercase name.
    return n->nodeName().qstring(); 
}

static QTextStream &operator<<(QTextStream &ts, const RenderObject &o)
{
    ts << o.renderName();
    
    if (o.style() && o.style()->zIndex()) {
        ts << " zI: " << o.style()->zIndex();
    }
    
    if (o.element()) {
        QString tagName = getTagName(o.element());
        if (!tagName.isEmpty()) {
            ts << " {" << tagName << "}";
        }
    }
    
    // FIXME: Will remove this <br> code once all layout tests pass.  Until then, we can't really change
    // all the results easily.
    bool usePositions = true;
    if (o.isBR()) {
        const RenderBR* br = static_cast<const RenderBR*>(&o);
        usePositions = (br->firstTextBox() && br->firstTextBox()->isText());
    }
    QRect r(usePositions ? o.xPos() : 0, usePositions ? o.yPos() : 0, o.width(), o.height());
    ts << " " << r;
    
    if (!o.isText()) {
        if (o.parent() && (o.parent()->style()->color() != o.style()->color()))
            ts << " [color=" << o.style()->color().name() << "]";
        if (o.parent() && (o.parent()->style()->backgroundColor() != o.style()->backgroundColor()) &&
            o.style()->backgroundColor().isValid() && 
	    o.style()->backgroundColor().rgb() != khtml::transparentColor)
	    // Do not dump invalid or transparent backgrounds, since that is the default.
            ts << " [bgcolor=" << o.style()->backgroundColor().name() << "]";
    
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
                    QColor col = o.style()->borderTopColor();
                    if (!col.isValid()) col = o.style()->color();
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
                    QColor col = o.style()->borderRightColor();
                    if (!col.isValid()) col = o.style()->color();
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
                    QColor col = o.style()->borderBottomColor();
                    if (!col.isValid()) col = o.style()->color();
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
                    QColor col = o.style()->borderLeftColor();
                    if (!col.isValid()) col = o.style()->color();
                    ts << col.name() << ")";
                }
            }
            
            ts << "]";
        }
    }
    
    if (o.isTableCell()) {
        const RenderTableCell &c = static_cast<const RenderTableCell &>(o);
        ts << " [r=" << c.row() << " c=" << c.col() << " rs=" << c.rowSpan() << " cs=" << c.colSpan() << "]";
    }

    return ts;
}

static QString quoteAndEscapeNonPrintables(const QString &s)
{
    QString result;
    result += '"';
    for (uint i = 0; i != s.length(); ++i) {
        QChar c = s.at(i);
        if (c == '\\') {
            result += "\\\\";
        } else if (c == '"') {
            result += "\\\"";
        } else if (c == '\n' || c.unicode() == 0x00A0) {
            result += ' ';
        } else {
            ushort u = c.unicode();
            if (u >= 0x20 && u < 0x7F) {
                result += c;
            } else {
                QString hex;
                hex.sprintf("\\x{%X}", u);
                result += hex;
            }
        }
    }
    result += '"';
    return result;
}

static void writeTextRun(QTextStream &ts, const RenderText &o, const InlineTextBox &run)
{
    ts << "text run at (" << run.m_x << "," << run.m_y << ") width " << run.m_width << ": "
    	<< quoteAndEscapeNonPrintables(o.data().qstring().mid(run.m_start, run.m_len))
    	<< "\n"; 
}

void write(QTextStream &ts, const RenderObject &o, int indent)
{
#if SVG_SUPPORT
    // FIXME:  A hackish way to doing our own "virtual" dispatch
    if (o.isRenderPath()) {
        write(ts, static_cast<const RenderPath &>(o), indent);
        return;
    }
    if (o.isKCanvasContainer()) {
        write(ts, static_cast<const KCanvasContainer &>(o), indent);
        return;
    }
#endif
    writeIndent(ts, indent);
    
    ts << o << "\n";
    
    if (o.isText() && !o.isBR()) {
        const RenderText &text = static_cast<const RenderText &>(o);
        for (InlineTextBox* box = text.firstTextBox(); box; box = box->nextTextBox()) {
            writeIndent(ts, indent+1);
            writeTextRun(ts, text, *box);
        }
    }

    for (RenderObject *child = o.firstChild(); child; child = child->nextSibling()) {
        if (child->layer()) {
            continue;
        }
        write(ts, *child, indent + 1);
    }
    
    if (o.isWidget()) {
        QWidget *widget = static_cast<const RenderWidget &>(o).widget();
        if (widget && widget->inherits("KHTMLView")) {
            KHTMLView *view = static_cast<KHTMLView *>(widget);
            RenderObject *root = KWQ(view->part())->renderer();
            if (root) {
                view->layout();
                RenderLayer* l = root->layer();
                if (l)
                    writeLayers(ts, l, l, QRect(l->xPos(), l->yPos(), l->width(), l->height()), indent+1);
            }
        }
    }
}

static void write(QTextStream &ts, RenderLayer &l,
                  const QRect& layerBounds, const QRect& backgroundClipRect, const QRect& clipRect, const QRect& outlineClipRect,
                  int layerType = 0, int indent = 0)
{
    writeIndent(ts, indent);
    
    ts << "layer";
    ts << " " << layerBounds;

    if (layerBounds != layerBounds.intersect(backgroundClipRect))
        ts << " backgroundClip " << backgroundClipRect;
    if (layerBounds != layerBounds.intersect(clipRect))
        ts << " clip " << clipRect;
    if (layerBounds != layerBounds.intersect(outlineClipRect))
        ts << " outlineClip " << outlineClipRect;

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
    
static void writeLayers(QTextStream &ts, const RenderLayer* rootLayer, RenderLayer* l,
                        const QRect& paintDirtyRect, int indent)
{
    // Calculate the clip rects we should use.
    QRect layerBounds, damageRect, clipRectToApply, outlineRect;
    l->calculateRects(rootLayer, paintDirtyRect, layerBounds, damageRect, clipRectToApply, outlineRect);
    
    // Ensure our z-order lists are up-to-date.
    l->updateZOrderLists();

    bool shouldPaint = l->intersectsDamageRect(layerBounds, damageRect);
    QPtrVector<RenderLayer>* negList = l->negZOrderList();
    if (shouldPaint && negList && negList->count() > 0)
        write(ts, *l, layerBounds, damageRect, clipRectToApply, outlineRect, -1, indent);

    if (negList) {
        for (unsigned i = 0; i != negList->count(); ++i)
            writeLayers(ts, rootLayer, negList->at(i), paintDirtyRect, indent);
    }

    if (shouldPaint)
        write(ts, *l, layerBounds, damageRect, clipRectToApply, outlineRect, negList && negList->count() > 0, indent);

    QPtrVector<RenderLayer>* posList = l->posZOrderList();
    if (posList) {
        for (unsigned i = 0; i != posList->count(); ++i)
            writeLayers(ts, rootLayer, posList->at(i), paintDirtyRect, indent);
    }
}

static QString nodePosition(NodeImpl *node)
{
    QString result;

    NodeImpl *p;
    for (NodeImpl *n = node; n; n = p) {
        p = n->parentNode();
        if (n != node)
            result += " of ";
        if (p)
            result += "child " + QString::number(n->nodeIndex()) + " {" + getTagName(n) + "}";
        else
            result += "document";
    }

    return result;
}

static void writeSelection(QTextStream &ts, const RenderObject *o)
{
    NodeImpl *n = o->element();
    if (!n || !n->isDocumentNode())
        return;

    DocumentImpl *doc = static_cast<DocumentImpl *>(n);
    KHTMLPart *part = doc->part();
    if (!part)
        return;

    SelectionController selection = part->selection();
    if (selection.isCaret()) {
        ts << "caret: position " << selection.start().offset() << " of " << nodePosition(selection.start().node());
        if (selection.startAffinity() == UPSTREAM)
            ts << " (upstream affinity)";
        ts << "\n"; 
    } else if (selection.isRange()) {
        ts << "selection start: position " << selection.start().offset() << " of " << nodePosition(selection.start().node()) << "\n"
           << "selection end:   position " << selection.end().offset() << " of " << nodePosition(selection.end().node()) << "\n"; 
    }
}

static bool debuggingRenderTreeFlag = false;

bool debuggingRenderTree()
{
    return debuggingRenderTreeFlag;
}

QString externalRepresentation(RenderObject *o)
{
    debuggingRenderTreeFlag = true;
    JSEditor::setSupportsPasteCommand(true);

    QString s;
    {
        QTextStream ts(&s);
#if SVG_SUPPORT
        ts.precision(2);
#endif
        if (o) {
            o->canvas()->view()->layout();
            RenderLayer* l = o->layer();
            if (l) {
                writeLayers(ts, l, l, QRect(l->xPos(), l->yPos(), l->width(), l->height()));
                writeSelection(ts, o);
            }
        }
    }
    return s;
}
