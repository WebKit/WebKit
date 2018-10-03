/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "HTMLAppletElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOptionElement.h"

namespace WebCore {

using namespace HTMLNames;

template <CollectionTraversalType traversalType>
bool GenericCachedHTMLCollection<traversalType>::elementMatches(Element& element) const
{
    switch (this->type()) {
    case NodeChildren:
        return true;
    case DocImages:
        return element.hasTagName(imgTag);
    case DocScripts:
        return element.hasTagName(scriptTag);
    case DocForms:
        return element.hasTagName(formTag);
    case TableTBodies:
        return element.hasTagName(tbodyTag);
    case TRCells:
        return element.hasTagName(tdTag) || element.hasTagName(thTag);
    case TSectionRows:
        return element.hasTagName(trTag);
    case SelectedOptions:
        return is<HTMLOptionElement>(element) && downcast<HTMLOptionElement>(element).selected();
    case DataListOptions:
        if (is<HTMLOptionElement>(element)) {
            HTMLOptionElement& option = downcast<HTMLOptionElement>(element);
            if (!option.isDisabledFormControl() && !option.value().isEmpty())
                return true;
        }
        return false;
    case MapAreas:
        return element.hasTagName(areaTag);
    case DocApplets:
        return is<HTMLAppletElement>(element) || (is<HTMLObjectElement>(element) && downcast<HTMLObjectElement>(element).containsJavaApplet());
    case DocEmbeds:
        return element.hasTagName(embedTag);
    case DocLinks:
        return (element.hasTagName(aTag) || element.hasTagName(areaTag)) && element.hasAttributeWithoutSynchronization(hrefAttr);
    case DocAnchors:
        return element.hasTagName(aTag) && element.hasAttributeWithoutSynchronization(nameAttr);
    case FieldSetElements:
        return is<HTMLObjectElement>(element) || is<HTMLFormControlElement>(element);
    case ByClass:
    case ByTag:
    case ByHTMLTag:
    case AllDescendants:
    case DocAll:
    case DocumentAllNamedItems:
    case DocumentNamedItems:
    case FormControls:
    case SelectOptions:
    case TableRows:
    case WindowNamedItems:
        break;
    }
    // Remaining collection types have their own CachedHTMLCollection subclasses and are not using GenericCachedHTMLCollection.
    ASSERT_NOT_REACHED();
    return false;
}
template bool GenericCachedHTMLCollection<CollectionTraversalType::Descendants>::elementMatches(Element&) const;
template bool GenericCachedHTMLCollection<CollectionTraversalType::ChildrenOnly>::elementMatches(Element&) const;

} // namespace WebCore
