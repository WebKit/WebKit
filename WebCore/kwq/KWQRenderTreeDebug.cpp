/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "KWQRenderTreeDebug.h"

#include "htmltags.h"
#include "khtmlview.h"
#include "render_replaced.h"
#include "render_table.h"
#include "render_text.h"

#include "KWQKHTMLPart.h"
#include "KWQTextStream.h"

using khtml::RenderLayer;
using khtml::RenderObject;
using khtml::RenderTableCell;
using khtml::RenderWidget;
using khtml::RenderText;
using khtml::TextRun;
using khtml::TextRunArray;

typedef khtml::RenderLayer::RenderLayerElement RenderLayerElement;
typedef khtml::RenderLayer::RenderZTreeNode RenderZTreeNode;

static void writeLayers(QTextStream &ts, const RenderObject &o, int indent = 0);

static QTextStream &operator<<(QTextStream &ts, const QRect &r)
{
    return ts << "at (" << r.x() << "," << r.y() << ") size " << r.width() << "x" << r.height();
}

static void writeIndent(QTextStream &ts, int indent)
{
    for (int i = 0; i != indent; ++i) {
        ts << "  ";
    }
}

static QTextStream &operator<<(QTextStream &ts, const RenderObject &o)
{
    ts << o.renderName();
    
    if (o.style() && o.style()->zIndex()) {
        ts << " zI: " << o.style()->zIndex();
    }
    
    if (o.element()) {
        QString tagName(getTagName(o.element()->id()).string());
        if (!tagName.isEmpty()) {
            ts << " {" << tagName << "}";
        }
    }
    
    QRect r(o.xPos(), o.yPos(), o.width(), o.height());
    ts << " " << r;
    
    if (o.parent() && (o.parent()->style()->color() != o.style()->color()))
        ts << " [color=" << o.style()->color().name() << "]";
    if (o.parent() && (o.parent()->style()->backgroundColor() != o.style()->backgroundColor()))
        ts << " [bgcolor=" << o.style()->backgroundColor().name() << "]";

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

static void writeTextRun(QTextStream &ts, const RenderText &o, const TextRun &run)
{
    ts << "text run at (" << run.m_x << "," << run.m_y << ") width " << run.m_width << ": "
    	<< quoteAndEscapeNonPrintables(o.data().string().mid(run.m_start, run.m_len))
    	<< "\n"; 
}

static void write(QTextStream &ts, const RenderObject &o, int indent = 0)
{
    writeIndent(ts, indent);
    
    ts << o << "\n";
    
    if (o.isText()) {
        const RenderText &text = static_cast<const RenderText &>(o);
        TextRunArray runs = text.textRuns();
        for (unsigned int i = 0; i < runs.count(); i++) {
            writeIndent(ts, indent+1);
            writeTextRun(ts, text, *runs[i]);
        }
    }

    for (RenderObject *child = o.firstChild(); child; child = child->nextSibling()) {
        if (child->layer()) {
            continue;
        }
        write(ts, *child, indent + 1);
    }
    
    if (o.isWidget()) {
        KHTMLView *view = dynamic_cast<KHTMLView *>(static_cast<const RenderWidget &>(o).widget());
        if (view) {
            RenderObject *root = KWQ(view->part())->renderer();
            if (root) {
                writeLayers(ts, *root, indent + 1);
            }
        }
    }
}

static void write(QTextStream &ts, const RenderLayerElement &e, int indent = 0)
{
    RenderLayer &l = *e.layer;
    
    writeIndent(ts, indent);
    
    ts << "layer";
    
    QRect r(l.xPos(), l.yPos(), l.width(), l.height());
    
    ts << " " << r;
    
    if (r != r.intersect(e.backgroundClipRect)) {
        ts << " backgroundClip " << e.backgroundClipRect;
    }
    if (r != r.intersect(e.clipRect)) {
        ts << " clip " << e.clipRect;
    }

    if (e.layerElementType == RenderLayerElement::Background)
        ts << " layerType: background only";
    else if (e.layerElementType == RenderLayerElement::Foreground)
        ts << " layerType: foreground only";
    
    ts << "\n";

    if (e.layerElementType != RenderLayerElement::Background)
        write(ts, *l.renderer(), indent + 1);
}

static void writeLayers(QTextStream &ts, const RenderObject &o, int indent)
{
    RenderZTreeNode *node;
    QPtrVector<RenderLayerElement> list = o.layer()->elementList(node);
    for (unsigned i = 0; i != list.count(); ++i) {
        write(ts, *list[i], indent);
    }
    if (node) {
        node->detach(o.renderArena());
    }
}

QString externalRepresentation(const RenderObject *o)
{
    QString s;
    {
        QTextStream ts(&s);
        if (o) {
            writeLayers(ts, *o);
        }
    }
    return s;
}
