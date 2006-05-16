/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
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

#include "DeprecatedString.h"
#include "KWQTextStream.h"
#include "DeprecatedValueList.h"

class QMatrix;
class DeprecatedStringList;

namespace WebCore {
    class KCClipData;
    class KCPathData;
    class Color;
    class FloatSize;
    class FloatPoint;
    class FloatRect;
    class IntPoint;
    class IntRect;
    class Node;
    class RenderPath;
    class KCanvasContainer;

// functions used by the main RenderTreeAsText code
void write(QTextStream &ts, const WebCore::KCanvasContainer &container, int indent = 0);
void write(QTextStream &ts, const WebCore::RenderPath &path, int indent = 0);
void writeRenderResources(QTextStream &ts, WebCore::Node *parent);

// helper operators defined used in various classes to dump the render tree. 
QTextStream &operator<<(QTextStream &ts, const QMatrix &);
QTextStream &operator<<(QTextStream &ts, const WebCore::IntRect &);
QTextStream &operator<<(QTextStream &ts, const WebCore::Color &);
QTextStream &operator<<(QTextStream &ts, const WebCore::IntPoint &);
QTextStream &operator<<(QTextStream &ts, const WebCore::FloatSize &);
QTextStream &operator<<(QTextStream &ts, const WebCore::FloatRect &);
QTextStream &operator<<(QTextStream &ts, const WebCore::FloatPoint &);

// helper operators specific to dumping the render tree. these are used in various classes to dump the render tree
// these could be defined in separate namespace to avoid matching these generic signatures unintentionally.
    
QTextStream &operator<<(QTextStream &ts, const DeprecatedStringList &l);
    
template<typename Item>
QTextStream &operator<<(QTextStream &ts, const DeprecatedValueList<Item*> &l)
{
    ts << "[";
    typename DeprecatedValueList<Item*>::ConstIterator it = l.begin();
    typename DeprecatedValueList<Item*>::ConstIterator it_e = l.end();
    while (it != it_e)
    {
        ts << *(*it);
        ++it;
        if (it != it_e) ts << ", ";
    }
    ts << "]";
    
    return ts;
}

template<typename Item>
QTextStream &operator<<(QTextStream &ts, const DeprecatedValueList<Item> &l)
{
    ts << "[";
    typename DeprecatedValueList<Item>::ConstIterator it = l.begin();
    typename DeprecatedValueList<Item>::ConstIterator it_e = l.end();
    while (it != it_e)
    {
        ts << *it;
        ++it;
        if (it != it_e) ts << ", ";
    }
    ts << "]";
    
    return ts;
}

}
