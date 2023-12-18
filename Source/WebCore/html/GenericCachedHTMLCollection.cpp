/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GenericCachedHTMLCollection.h"

#include "CachedHTMLCollectionInlines.h"
#include "HTMLFieldSetElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOptionElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

using GenericDescendantsCachedHTMLCollection = GenericCachedHTMLCollection<CollectionTraversalType::Descendants>;
using GenericChildrenOnlyCachedHTMLCollection = GenericCachedHTMLCollection<CollectionTraversalType::ChildrenOnly>;

WTF_MAKE_ISO_ALLOCATED_IMPL_TEMPLATE(GenericDescendantsCachedHTMLCollection);
WTF_MAKE_ISO_ALLOCATED_IMPL_TEMPLATE(GenericChildrenOnlyCachedHTMLCollection);

template <CollectionTraversalType traversalType>
GenericCachedHTMLCollection<traversalType>::GenericCachedHTMLCollection(ContainerNode& base, CollectionType collectionType)
    : CachedHTMLCollection<GenericCachedHTMLCollection<traversalType>, traversalType>(base, collectionType)
{ }
template GenericCachedHTMLCollection<CollectionTraversalType::Descendants>::GenericCachedHTMLCollection(ContainerNode&, CollectionType);
template GenericCachedHTMLCollection<CollectionTraversalType::ChildrenOnly>::GenericCachedHTMLCollection(ContainerNode&, CollectionType);

template <CollectionTraversalType traversalType>
bool GenericCachedHTMLCollection<traversalType>::elementMatches(Element& element) const
{
    switch (this->type()) {
    case CollectionType::NodeChildren:
        return true;
    case CollectionType::DocImages:
        return element.hasTagName(imgTag);
    case CollectionType::DocScripts:
        return element.hasTagName(scriptTag);
    case CollectionType::DocForms:
        return element.hasTagName(formTag);
    case CollectionType::TableTBodies:
        return element.hasTagName(tbodyTag);
    case CollectionType::TRCells:
        return element.hasTagName(tdTag) || element.hasTagName(thTag);
    case CollectionType::TSectionRows:
        return element.hasTagName(trTag);
    case CollectionType::SelectedOptions: {
        auto* optionElement = dynamicDowncast<HTMLOptionElement>(element);
        return optionElement && optionElement->selected();
    }
    case CollectionType::DataListOptions:
        return is<HTMLOptionElement>(element);
    case CollectionType::MapAreas:
        return element.hasTagName(areaTag);
    case CollectionType::DocEmbeds:
        return element.hasTagName(embedTag);
    case CollectionType::DocLinks:
        return (element.hasTagName(aTag) || element.hasTagName(areaTag)) && element.hasAttributeWithoutSynchronization(hrefAttr);
    case CollectionType::DocAnchors:
        return element.hasTagName(aTag) && element.hasAttributeWithoutSynchronization(nameAttr);
    case CollectionType::FieldSetElements:
        return element.isFormListedElement();
    case CollectionType::ByClass:
    case CollectionType::ByTag:
    case CollectionType::ByHTMLTag:
    case CollectionType::AllDescendants:
    case CollectionType::DocAll:
    case CollectionType::DocEmpty:
    case CollectionType::DocumentAllNamedItems:
    case CollectionType::DocumentNamedItems:
    case CollectionType::FormControls:
    case CollectionType::SelectOptions:
    case CollectionType::TableRows:
    case CollectionType::WindowNamedItems:
        break;
    }
    // Remaining collection types have their own CachedHTMLCollection subclasses and are not using GenericCachedHTMLCollection.
    ASSERT_NOT_REACHED();
    return false;
}
template bool GenericCachedHTMLCollection<CollectionTraversalType::Descendants>::elementMatches(Element&) const;
template bool GenericCachedHTMLCollection<CollectionTraversalType::ChildrenOnly>::elementMatches(Element&) const;

} // namespace WebCore
