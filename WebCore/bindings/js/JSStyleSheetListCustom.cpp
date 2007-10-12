
/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSStyleSheetList.h"

#include "HTMLStyleElement.h"
#include "JSStyleSheet.h"
#include "StyleSheet.h"
#include "StyleSheetList.h"

namespace WebCore {

bool JSStyleSheetList::canGetItemsForName(KJS::ExecState*, StyleSheetList* styleSheetList, const KJS::Identifier& propertyName)
{
    return styleSheetList->getNamedItem(propertyName);
}

KJS::JSValue* JSStyleSheetList::nameGetter(KJS::ExecState* exec, KJS::JSObject*, const KJS::Identifier& propertyName, const KJS::PropertySlot& slot)
{
    JSStyleSheetList* thisObj = static_cast<JSStyleSheetList*>(slot.slotBase());
    HTMLStyleElement* element = thisObj->impl()->getNamedItem(propertyName);
    ASSERT(element);
    return toJS(exec, element->sheet());
}

} // namespace WebCore
