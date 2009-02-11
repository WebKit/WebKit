/*
 * Copyright (C) Kevin Ollivier  All rights reserved.
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

#ifndef InspectorClientWx_h
#define InspectorClientWx_h

#include "InspectorClient.h"

namespace WebCore {

class Node;
class Page;
class String;

class InspectorClientWx : public InspectorClient {
public:
    InspectorClientWx();
    ~InspectorClientWx();

    virtual void inspectorDestroyed();

    virtual Page* createPage();

    virtual String localizedStringsURL();

    virtual String hiddenPanels();

    virtual void showWindow();
    virtual void closeWindow();

    virtual void attachWindow();
    virtual void detachWindow();

    virtual void setAttachedWindowHeight(unsigned height);

    virtual void highlight(Node*);
    virtual void hideHighlight();

    virtual void inspectedURLChanged(const String& newURL);

    virtual void populateSetting(const String& key, InspectorController::Setting&);
    virtual void storeSetting(const String& key, const InspectorController::Setting&);
    virtual void removeSetting(const String& key);
};

} // namespace WebCore

#endif // !defined(InspectorClient_h)
