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

#ifndef __jsediting_h__
#define __jsediting_h__

#include "dom_docimpl.h"
#include "dom_string.h"
#include "qdict.h"

class KHTMLPart;
class QString;

namespace DOM {

class DocumentImpl;

class JSEditor {

public:
    JSEditor(DocumentImpl *);

    typedef bool (JSEditor::*execCommandFn)(bool userInterface, const DOMString &value);
    typedef bool (JSEditor::*queryBoolFn)();
    typedef DOM::DOMString (JSEditor::*queryValueFn)();

    struct CommandIdentifier {
        CommandIdentifier() {}
        CommandIdentifier(execCommandFn exec, queryBoolFn enabled, queryBoolFn indeterm, queryBoolFn state, queryBoolFn supported, queryValueFn value)
            : execFn(exec), enabledFn(enabled), indetermFn(indeterm), stateFn(state), supportedFn(supported), valueFn(value) {}

        JSEditor::execCommandFn execFn;
        queryBoolFn enabledFn;
        queryBoolFn indetermFn;
        queryBoolFn stateFn;
        queryBoolFn supportedFn;
        queryValueFn valueFn;
    };
    
    bool execCommand(const DOMString &command, bool userInterface, const DOMString &value);
    bool queryCommandEnabled(const DOMString &command);
    bool queryCommandIndeterm(const DOMString &command);
    bool queryCommandState(const DOMString &command);
    bool queryCommandSupported(const DOMString &command);
    DOM::DOMString queryCommandValue(const DOMString &command);

private:
    JSEditor(const JSEditor &);
    JSEditor &operator=(const JSEditor &);

    // execCommand implementations
    bool execCommandCopy(bool userInterface, const DOMString &value);
    bool execCommandCut(bool userInterface, const DOMString &value);
    bool execCommandDelete(bool userInterface, const DOMString &value);
    bool execCommandInsertText(bool userInterface, const DOMString &value);
    bool execCommandPaste(bool userInterface, const DOMString &value);
    bool execCommandRedo(bool userInterface, const DOMString &value);
    bool execCommandSelectAll(bool userInterface, const DOMString &value);
    bool execCommandUndo(bool userInterface, const DOMString &value);

    // queryCommandEnabled implementations
    bool enabledIfPartNotNull();
    bool enabledIfSelectionNotEmpty();
    bool enabledIfSelectionIsRange();

    // queryCommandSupported implementations
    bool commandSupported();

    QDict<JSEditor::CommandIdentifier> &commandDict();
    DocumentImpl *document() { return m_doc; }
    KHTMLPart *part() { return document()->part(); }
    CommandIdentifier *commandIdentifier(const DOMString &command);

    void addCommand(const QString &cmd, execCommandFn exec, queryBoolFn enabled, queryBoolFn indeterm, queryBoolFn state, queryBoolFn supported, queryValueFn value);
    void initDict();

    DocumentImpl *m_doc;
};

} // end namespace DOM

#endif
