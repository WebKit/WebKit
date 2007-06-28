/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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


#ifndef DOMSelection_h
#define DOMSelection_h

#include "Shared.h"
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

    class Frame;
    class Range;
    class Node;
    class String;

    typedef int ExceptionCode;

    class DOMSelection : public Shared<DOMSelection> {
    public:
        DOMSelection(Frame*);

        Frame* frame() const;
        void disconnectFrame();

        Node* anchorNode() const;
        Node* baseNode() const;
        int anchorOffset() const;
        int baseOffset() const;
        Node* focusNode() const;
        Node* extentNode() const;
        int focusOffset() const;
        int extentOffset() const;
        bool isCollapsed() const;
        String type() const;
        int rangeCount() const;

        void collapse(Node*, int offset, ExceptionCode&);
        void collapseToEnd();
        void collapseToStart();
        void empty();
        void setBaseAndExtent(Node* baseNode, int baseOffset, Node* extentNode, int extentOffset, ExceptionCode&);
        void setPosition(Node*, int offset, ExceptionCode&);
        void setPosition(Node*, ExceptionCode&);
        void modify(const String& alter, const String& direction, const String& granularity);
        PassRefPtr<Range> getRangeAt(int, ExceptionCode&);
        void removeAllRanges();
        void addRange(Range*);

        String toString();

    private:
        Frame* m_frame;
    };

} // namespace WebCore

#endif // DOMSelection_h
