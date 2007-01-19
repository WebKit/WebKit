/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef FrameMac_h
#define FrameMac_h

#import "Frame.h"

class NPObject;

namespace KJS {
    namespace Bindings {
        class Instance;
        class RootObject;
    }
}

#ifdef __OBJC__

@class NSArray;
@class NSDictionary;
@class NSFont;
@class NSImage;
@class NSMenu;
@class NSMutableDictionary;
@class NSString;
@class WebCoreFrameBridge;
@class WebScriptObject;

#else

class NSArray;
class NSDictionary;
class NSFont;
class NSImage;
class NSMenu;
class NSMutableDictionary;
class NSString;
class WebCoreFrameBridge;
class WebScriptObject;

typedef unsigned int NSDragOperation;
typedef int NSWritingDirection;

#endif

namespace WebCore {

class HTMLTableCellElement;

class FrameMac : public Frame {
    friend class Frame;

public:
    FrameMac(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);
    ~FrameMac();

    void setBridge(WebCoreFrameBridge*);
    WebCoreFrameBridge* bridge() const { return _bridge; }

private:    
    WebCoreFrameBridge* _bridge;

// === undecided, may or may not belong here

public:
    NSString* searchForLabelsAboveCell(RegularExpression*, HTMLTableCellElement*);
    NSString* searchForLabelsBeforeElement(NSArray* labels, Element*);
    NSString* matchLabelsAgainstElement(NSArray* labels, Element*);

    virtual KJS::Bindings::Instance* getEmbedInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getObjectInstanceForWidget(Widget*);
    virtual KJS::Bindings::Instance* getAppletInstanceForWidget(Widget*);
    virtual KJS::Bindings::RootObject* bindingRootObject();

    void addPluginRootObject(KJS::Bindings::RootObject*);

    KJS::Bindings::RootObject* rootObjectForDOM();
    
    WebScriptObject* windowScriptObject();
    NPObject* windowScriptNPObject();
    
    NSMutableDictionary* dashboardRegionsDictionary();
    void dashboardRegionsChanged();

    void willPopupMenu(NSMenu *);

    void cleanupPluginObjects();

    NSImage* selectionImage(bool forceWhiteText = false) const;
    NSImage* snapshotDragImage(Node*, NSRect* imageRect, NSRect* elementRect) const;

private:    
    NSImage* imageFromRect(NSRect) const;

    KJS::Bindings::RootObject* _bindingRootObject; // The root object used for objects bound outside the context of a plugin.
    Vector<KJS::Bindings::RootObject*> m_rootObjects;
    WebScriptObject* _windowScriptObject;
    NPObject* _windowScriptNPObject;

// === to be moved into Chrome

public:
    virtual void focusWindow();
    virtual void unfocusWindow();

    virtual bool shouldInterruptJavaScript();    

    FloatRect customHighlightLineRect(const AtomicString& type, const FloatRect& lineRect);
    void paintCustomHighlight(const AtomicString& type, const FloatRect& boxRect, const FloatRect& lineRect, bool text, bool line);

    virtual void print();

// === to be moved into Editor

public:

    NSDictionary* fontAttributesForSelectionStart() const;
    
    NSWritingDirection baseWritingDirectionForSelectionStart() const;

    virtual void issueCutCommand();
    virtual void issueCopyCommand();
    virtual void issuePasteCommand();
    virtual void issuePasteAndMatchStyleCommand();
    virtual void issueTransposeCommand();
    virtual void respondToChangedSelection(const Selection& oldSelection, bool closeTyping);
    virtual bool isContentEditable() const;
    virtual bool shouldChangeSelection(const Selection& oldSelection, const Selection& newSelection, EAffinity affinity, bool stillSelecting) const;
    virtual bool shouldDeleteSelection(const Selection&) const;
    
    virtual void setSecureKeyboardEntry(bool);
    virtual bool isSecureKeyboardEntry();

    void setMarkedTextRange(const Range* , NSArray* attributes, NSArray* ranges);
    virtual Range* markedTextRange() const { return m_markedTextRange.get(); }

    virtual void textFieldDidBeginEditing(Element*);
    virtual void textFieldDidEndEditing(Element*);
    virtual void textDidChangeInTextField(Element*);
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*);
    virtual void textWillBeDeletedInTextField(Element*);
    virtual void textDidChangeInTextArea(Element*);
    
private:
    RefPtr<Range> m_markedTextRange;
    
// === to be moved into the Platform directory

public:
    virtual String mimeTypeForFileName(const String&) const;
    virtual bool isCharacterSmartReplaceExempt(UChar, bool);
    
};

inline FrameMac* Mac(Frame* frame) { return static_cast<FrameMac*>(frame); }
inline const FrameMac* Mac(const Frame* frame) { return static_cast<const FrameMac*>(frame); }

}

#endif
