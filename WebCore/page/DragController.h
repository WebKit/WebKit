/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef DragController_h
#define DragController_h

#include "DragActions.h"

namespace WebCore {

    class Clipboard;
    class Document;
    class DragClient;
    class DragData;
    class SelectionController;
    class Page;
    
    class DragController {
    public:
        DragController(Page*, DragClient*);
        ~DragController();
        DragClient* client() const { return m_client; }

        DragOperation dragEntered(DragData*);
        void dragExited(DragData*);
        DragOperation dragUpdated(DragData*);
        bool performDrag(DragData*);
        
        //FIXME: It should be possible to remove a number of these accessors once all
        //drag logic is in WebCore
        void setDidInitiateDrag(bool initiated) { m_didInitiateDrag = initiated; } 
        bool didInitiateDrag() const { return m_didInitiateDrag; }
        void setIsHandlingDrag(bool handling) { m_isHandlingDrag = handling; }
        bool isHandlingDrag() const { return m_isHandlingDrag; }
        void setDragOperation(DragOperation dragOp) { m_dragOperation = dragOp; }
        DragOperation dragOperation() const { return m_dragOperation; }        
        void setDragInitiator(Document* initiator) { m_dragInitiator = initiator; m_didInitiateDrag = true; }
        Document* dragInitiator() const { return m_dragInitiator; }
        void setDragSourceAction(DragSourceAction action) { m_dragSourceAction = action; }
        DragSourceAction dragSourceAction() const { return m_dragSourceAction; }
        
        Document* document() const { return m_document; }
        DragDestinationAction dragDestinationAction() const { return m_dragDestinationAction; }
        
        void dragEnded() { m_dragInitiator = 0; m_didInitiateDrag = false; }
        
    private:
        bool canProcessDrag(DragData*);
        bool concludeDrag(DragData*, DragDestinationAction);
        DragOperation dragEnteredOrUpdated(DragData*);
        DragOperation operationForLoad(DragData*);
        DragOperation tryDocumentDrag(DragData*, DragDestinationAction);
        DragOperation tryDHTMLDrag(DragData*);
        DragOperation dragOperation(DragData*);
        void cancelDrag();
        bool dragIsMove(SelectionController*, DragData*);
        bool isCopyKeyDown();
        
        Page* m_page;
        DragClient* m_client;
        
        //The Document the mouse was last dragged over
        Document* m_document;
        
        //The Document (if any) that initiated the drag
        Document* m_dragInitiator;
        
        DragDestinationAction m_dragDestinationAction;
        DragSourceAction m_dragSourceAction;
        bool m_didInitiateDrag;
        bool m_isHandlingDrag;
        DragOperation m_dragOperation;
    };

}

#endif
