/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "HTMLIntentElement.h"

#if ENABLE(WEB_INTENTS_TAG)

#include "Chrome.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLNames.h"
#include "Page.h"
#include "QualifiedName.h"

namespace WebCore {

using namespace HTMLNames;

HTMLIntentElement::HTMLIntentElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(intentTag));
}

PassRefPtr<HTMLIntentElement> HTMLIntentElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLIntentElement(tagName, document));
}

Node::InsertionNotificationRequest HTMLIntentElement::insertedInto(Node* insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);

    if (insertionPoint->inDocument() && document()->frame()) {
        KURL url = document()->completeURL(fastGetAttribute(hrefAttr));
        document()->frame()->loader()->client()->registerIntentService(
            fastGetAttribute(actionAttr), fastGetAttribute(typeAttr), url, fastGetAttribute(titleAttr), fastGetAttribute(dispositionAttr));
    }

    return InsertionDone;
}

}

#endif // ENABLE(WEB_INTENTS_TAG)
