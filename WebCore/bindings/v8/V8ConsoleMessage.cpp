/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "V8ConsoleMessage.h"

#include "Console.h"
#include "DOMWindow.h"
#include "Frame.h"
#include "OwnPtr.h"
#include "Page.h"
#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

Vector<V8ConsoleMessage>* V8ConsoleMessage::m_delayedMessages = 0;

V8ConsoleMessage::V8ConsoleMessage(const String& string, const String& sourceID, unsigned lineNumber)
     : m_string(string)
     , m_sourceID(sourceID)
     , m_lineNumber(lineNumber)
{
}

void V8ConsoleMessage::dispatchNow(Page* page)
{
    ASSERT(page);

    // Process any delayed messages to make sure that messages
    // appear in the right order in the console.
    processDelayed();

    Console* console = page->mainFrame()->domWindow()->console();
    console->addMessage(JSMessageSource, LogMessageType, ErrorMessageLevel, m_string, m_lineNumber, m_sourceID);
}

void V8ConsoleMessage::dispatchLater()
{
    if (!m_delayedMessages) {
        // Allocate a vector for the delayed messages. Will be
        // deallocated when the delayed messages are processed
        // in processDelayed().
        m_delayedMessages = new Vector<V8ConsoleMessage>();
    }

    m_delayedMessages->append(*this);
}

void V8ConsoleMessage::processDelayed()
{
    if (!m_delayedMessages)
        return;

    // Take ownership of the delayed vector to avoid re-entrancy issues.
    OwnPtr<Vector<V8ConsoleMessage> > delayedMessages(m_delayedMessages);
    m_delayedMessages = 0;

    // If we have a delayed vector it cannot be empty.
    ASSERT(!delayedMessages->isEmpty());

    // Add the delayed messages to the page of the active
    // context. If that for some bizarre reason does not
    // exist, we clear the list of delayed messages to avoid
    // posting messages. We still deallocate the vector.
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    // Iterate through all the delayed messages and add them
    // to the console.
    const int size = delayedMessages->size();
    for (int i = 0; i < size; ++i)
        delayedMessages->at(i).dispatchNow(page);
}

void V8ConsoleMessage::handler(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    // Use the frame where JavaScript is called from.
    Frame* frame = V8Proxy::retrieveFrameForEnteredContext();
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;

    v8::Handle<v8::String> errorMessageString = message->Get();
    ASSERT(!errorMessageString.IsEmpty());
    String errorMessage = toWebCoreString(errorMessageString);

    v8::Handle<v8::Value> resourceName = message->GetScriptResourceName();
    bool useURL = resourceName.IsEmpty() || !resourceName->IsString();
    String resourceNameString = useURL ? frame->document()->url() : toWebCoreString(resourceName);
    V8ConsoleMessage consoleMessage(errorMessage, resourceNameString, message->GetLineNumber());
    consoleMessage.dispatchNow(page);
}

} // namespace WebCore
