/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
 *           (C) 2005 Rob Buis <buis@kde.org>
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

#include "KCanvasTreeDebug.h"

#include <kcanvas/KCanvasMatrix.h>
#include <kcanvas/KCanvasItem.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingStyle.h>

#include <ksvg2/impl/SVGStyledElementImpl.h>

#include <qtextstream.h>

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

static QTextStream &operator<<(QTextStream &ts, const KCanvasMatrix &m)
{
    ts << "{m=((" << m.a() << "," << m.b() << ")(" << m.c() << "," << m.d() << "))";
    ts << " t=(" << m.e() << "," << m.f() << ")}";
    return ts;
}

#define DIFFERS_FROM_PARENT(path) (o.parent() && (o.parent()->path != o.path))

static QTextStream &operator<<(QTextStream &ts, const KCanvasItem &o)
{
    ts << " " << o.bbox();
    
    if (DIFFERS_FROM_PARENT(style()->visible()) && !o.style()->visible())
        ts << " [HIDDEN]";
    if (DIFFERS_FROM_PARENT(style()->objectMatrix()))
        ts << " [transform=" << o.style()->objectMatrix() << "]";
    if (DIFFERS_FROM_PARENT(style()->colorInterpolation()))
        ts << " [color interpolation=" << o.style()->colorInterpolation() << "]";
    if (DIFFERS_FROM_PARENT(style()->imageRendering()))
        ts << " [image rendering=" << o.style()->imageRendering() << "]";
    if (DIFFERS_FROM_PARENT(style()->opacity()))
        ts << " [opacity=" << o.style()->opacity() << "]";
    if (o.style()->isStroked() && DIFFERS_FROM_PARENT(style()->strokePainter()))
        ts << " [stroke=" << o.style()->strokePainter() << "]";
    if (o.style()->isFilled() && DIFFERS_FROM_PARENT(style()->fillPainter()))
        ts << " [fill=" << o.style()->fillPainter() << "]";
    if (!o.style()->clipPaths().isEmpty())
        ts << " [clip paths=" << o.style()->clipPaths().join(", ") << "]";
    if (o.style()->hasMarkers()) {
        if (o.style()->startMarker())
            ts << " [start marker=" << o.style()->startMarker() << "]";
        if (o.style()->midMarker())
            ts << " [middle marker=" << o.style()->midMarker() << "]";
        if (o.style()->endMarker())
            ts << " [end marker=" << o.style()->endMarker() << "]";
    }
    if (DIFFERS_FROM_PARENT(style()->filter()))
        ts << " [filter=" << o.style()->filter() << "]";
    
    return ts;
}

static QString getTagName(void *node)
{
    KSVG::SVGStyledElementImpl *elem = static_cast<KSVG::SVGStyledElementImpl *>(node);
    if (elem)
        return elem->nodeName().string();
    return QString();
}

void write(QTextStream &ts, KCanvasItem *item, int indent = 0)
{
    if (item)
    {
        writeIndent(ts, indent);
        if(item->isContainer())
            ts << "KCanvasContainer";
        else
            ts << "KCanvasItem";
        
        if (item->userData()) {
            QString tagName = getTagName(item->userData());
            if (!tagName.isEmpty()) {
                ts << " {" << tagName << "}";
            }
        }
        
        ts << *item << endl;
        
        if(item->isContainer()) {
            KCanvasContainer *parent = static_cast<KCanvasContainer *>(item);
            for (KCanvasItem *child = parent->first(); child != NULL; child = child->next())
                write(ts, child, indent + 1);
        }
    }
}

QString externalRepresentation(KCanvasItem *item)
{
    QString s;
    {
        QTextStream ts(&s);
        write(ts, item);
    }
    return s;
}
