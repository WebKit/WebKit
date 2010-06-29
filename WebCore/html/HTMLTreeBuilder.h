/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef HTMLTreeBuilder_h
#define HTMLTreeBuilder_h

#include "Element.h"
#include "FragmentScriptingPermission.h"
#include "HTMLTokenizer.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

class AtomicHTMLToken;
class Document;
class DocumentFragment;
class Frame;
class HTMLToken;
class HTMLDocument;
class LegacyHTMLTreeBuilder;
class Node;

class HTMLTreeBuilder : public Noncopyable {
public:
    // FIXME: Replace constructors with create() functions returning PassOwnPtrs
    HTMLTreeBuilder(HTMLTokenizer*, HTMLDocument*, bool reportErrors);
    HTMLTreeBuilder(HTMLTokenizer*, DocumentFragment*, FragmentScriptingPermission);
    ~HTMLTreeBuilder();

    void setPaused(bool paused) { m_isPaused = paused; }
    bool isPaused() const { return m_isPaused; }

    // The token really should be passed as a const& since it's never modified.
    void constructTreeFromToken(HTMLToken&);
    // Must be called when parser is paused before calling the parser again.
    PassRefPtr<Element> takeScriptToProcess(int& scriptStartLine);

    // Done, close any open tags, etc.
    void finished();

    static HTMLTokenizer::State adjustedLexerState(HTMLTokenizer::State, const AtomicString& tagName, Frame*);

    // FIXME: This is a dirty, rotten hack to keep HTMLFormControlElement happy
    // until we stop using the legacy parser. DO NOT CALL THIS METHOD.
    LegacyHTMLTreeBuilder* legacyTreeBuilder() const { return m_legacyTreeBuilder.get(); }

private:
    // Represents HTML5 "insertion mode"
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/parsing.html#insertion-mode
    enum InsertionMode {
        InitialMode,
        BeforeHTMLMode,
        BeforeHeadMode,
        InHeadMode,
        InHeadNoscriptMode,
        AfterHeadMode,
        InBodyMode,
        TextMode,
        InTableMode,
        InTableTextMode,
        InCaptionMode,
        InColumnGroupMode,
        InTableBodyMode,
        InRowMode,
        InCellMode,
        InSelectMode,
        InSelectInTableMode,
        InForeignContentMode,
        AfterBodyMode,
        InFramesetMode,
        AfterFramesetMode,
        AfterAfterBodyMode,
        AfterAfterFramesetMode,
    };

    class ElementRecord : public Noncopyable {
    public:
        ElementRecord(PassRefPtr<Element> element, PassOwnPtr<ElementRecord> next)
            : m_element(element)
            , m_next(next)
        {
        }

        Element* element() const { return m_element.get(); }
        ElementRecord* next() const { return m_next.get(); }
        PassOwnPtr<ElementRecord> releaseNext() { return m_next.release(); }
        void setNext(PassOwnPtr<ElementRecord> next) { m_next = next; }

    private:
        RefPtr<Element> m_element;
        OwnPtr<ElementRecord> m_next;
    };

    class ElementStack : public Noncopyable {
    public:
        void pop()
        {
            top()->finishParsingChildren();
            m_top = m_top->releaseNext();
        }

        void push(PassRefPtr<Element> element)
        {
            m_top.set(new ElementRecord(element, m_top.release()));
            top()->beginParsingChildren();
        }

        Element* top() const
        {
            return m_top->element();
        }

        void remove(Element* element)
        {
            if (m_top->element() == element) {
                pop();
                return;
            }
            ElementRecord* pos = m_top.get();
            while (pos->next()) {
                if (pos->next()->element() == element) {
                    pos->setNext(pos->next()->releaseNext());
                    return;
                }
            }
        }

        bool contains(Element* element) const
        {
            for (ElementRecord* pos = m_top.get(); pos; pos = pos->next()) {
                if (pos->element() == element)
                    return true;
            }
            return false;
        }

    private:
        OwnPtr<ElementRecord> m_top;
    };

    void passTokenToLegacyParser(HTMLToken&);

    // Specialized functions for processing the different types of tokens.
    void processToken(AtomicHTMLToken&);
    void processDoctypeToken(AtomicHTMLToken&);
    void processStartTag(AtomicHTMLToken&);
    void processEndTag(AtomicHTMLToken&);
    void processComment(AtomicHTMLToken&);
    void processCharacter(AtomicHTMLToken&);
    void processEndOfFile(AtomicHTMLToken&);

    // Default processing for the different insertion modes.
    void processDefaultForInitialMode(AtomicHTMLToken&);
    void processDefaultForBeforeHTMLMode(AtomicHTMLToken&);
    void processDefaultForBeforeHeadMode(AtomicHTMLToken&);
    void processDefaultForInHeadMode(AtomicHTMLToken&);
    void processDefaultForInHeadNoscriptMode(AtomicHTMLToken&);
    void processDefaultForAfterHeadMode(AtomicHTMLToken&);

    bool processStartTagForInHead(AtomicHTMLToken&);

    template<typename ChildType>
    PassRefPtr<ChildType> attach(Node* parent, PassRefPtr<ChildType> prpChild)
    {
        RefPtr<ChildType> child = prpChild;
        parent->parserAddChild(child);
        // It's slightly unfortunate that we need to hold a reference to child
        // here to call attach().  We should investigate whether we can rely on
        // |parent| to hold a ref at this point.  In the common case (at least
        // for elements), however, we'll get to use this ref in the stack of
        // open elements.
        child->attach();
        return child.release();
    };

    void insertDoctype(AtomicHTMLToken&);
    void insertComment(AtomicHTMLToken&);
    void insertCommentOnDocument(AtomicHTMLToken&);
    void insertElement(AtomicHTMLToken&);
    void insertSelfClosingElement(AtomicHTMLToken&);
    void insertFormattingElement(AtomicHTMLToken&);
    void insertGenericRCDATAElement(AtomicHTMLToken&);
    void insertGenericRawTextElement(AtomicHTMLToken&);
    void insertScriptElement(AtomicHTMLToken&);
    void insertTextNode(AtomicHTMLToken&);

    void insertHTMLStartTagBeforeHTML(AtomicHTMLToken&);
    void insertHTMLStartTagInBody(AtomicHTMLToken&);

    PassRefPtr<Element> createElement(AtomicHTMLToken&);
    int indexOfLastOpenFormattingElementOrMarker() const;
    void reopenFormattingElementsAfterIndex(unsigned lastOpenElementIndex);
    void reconstructTheActiveFormattingElements();

    Element* currentElement() { return m_openElements.top(); }

    RefPtr<Element> m_headElement;
    RefPtr<Element> m_formElement;
    ElementStack m_openElements;

    class FormattingElementEntry {
    public:
        FormattingElementEntry(Element* element)
            : m_element(element)
        {
            ASSERT(element);
        }

        enum MarkerEntryType { MarkerEntry };
        FormattingElementEntry(MarkerEntryType)
        {
        }

        bool isMarker() const { return !m_element; }

        Element* element() const
        {
            // The fact that !m_element == isMarker is an implementation detail
            // callers should check isMarker() before calling element().
            ASSERT(m_element);
            return m_element.get();
        }

        void replaceElement(PassRefPtr<Element> element)
        {
            ASSERT(m_element); // Once a marker, always a marker.
            m_element = element;
        }

    private:
        RefPtr<Element> m_element;
    };

    Vector<FormattingElementEntry> m_activeFormattingElements;
    bool m_framesetOk;

    // FIXME: Implement error reporting.
    void parseError(AtomicHTMLToken&) { }

    void handleScriptStartTag();
    void handleScriptEndTag(Element*, int scriptStartLine);

    void setInsertionMode(InsertionMode value) { m_insertionMode = value; }
    InsertionMode insertionMode() const { return m_insertionMode; }

    static bool isScriptingFlagEnabled(Frame* frame);

    Document* m_document; // This is only used by the m_legacyParser for now.
    bool m_reportErrors;
    bool m_isPaused;

    InsertionMode m_insertionMode;
    InsertionMode m_originalInsertionMode;

    // HTML5 spec requires that we be able to change the state of the tokenizer
    // from within parser actions.
    HTMLTokenizer* m_tokenizer;

    // We're re-using logic from the old LegacyHTMLTreeBuilder while this class is being written.
    OwnPtr<LegacyHTMLTreeBuilder> m_legacyTreeBuilder;

    // These members are intentionally duplicated as the first set is a hack
    // on top of the legacy parser which will eventually be removed.
    RefPtr<Element> m_lastScriptElement; // FIXME: Hack for <script> support on top of the old parser.
    int m_lastScriptElementStartLine; // FIXME: Hack for <script> support on top of the old parser.

    RefPtr<Element> m_scriptToProcess; // <script> tag which needs processing before resuming the parser.
    int m_scriptToProcessStartLine; // Starting line number of the script tag needing processing.

    // FIXME: FragmentScriptingPermission is a HACK for platform/Pasteboard.
    // FragmentScriptingNotAllowed causes the Parser to remove children
    // from <script> tags (so javascript doesn't show up in pastes).
    FragmentScriptingPermission m_fragmentScriptingPermission;
    bool m_isParsingFragment;
};

}

#endif
