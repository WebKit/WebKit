/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#if ENABLE(DATALIST_ELEMENT)

#include "HTMLDataListElement.h"

#include "GenericCachedHTMLCollection.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "IdTargetObserverRegistry.h"
#include "NodeRareData.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLDataListElement);

inline HTMLDataListElement::HTMLDataListElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    document.incrementDataListElementCount();
}

HTMLDataListElement::~HTMLDataListElement()
{
    document().decrementDataListElementCount();
}

Ref<HTMLDataListElement> HTMLDataListElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLDataListElement(tagName, document));
}

void HTMLDataListElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    oldDocument.decrementDataListElementCount();
    newDocument.incrementDataListElementCount();
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
}

Ref<HTMLCollection> HTMLDataListElement::options()
{
    return ensureRareData().ensureNodeLists().addCachedCollection<GenericCachedHTMLCollection<CollectionTypeTraits<DataListOptions>::traversalType>>(*this, DataListOptions);
}

void HTMLDataListElement::optionElementChildrenChanged()
{
    treeScope().idTargetObserverRegistry().notifyObservers(getIdAttribute());
}

bool HTMLDataListElement::isSuggestion(const HTMLOptionElement& descendant)
{
    return !descendant.isDisabledFormControl() && !descendant.value().isEmpty();
}

} // namespace WebCore

#endif // ENABLE(DATALIST_ELEMENT)
