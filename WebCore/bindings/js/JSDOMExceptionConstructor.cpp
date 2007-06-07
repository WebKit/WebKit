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
#include "JSDOMExceptionConstructor.h"

#include "ExceptionCode.h"

#include "JSDOMExceptionConstructor.lut.h"

namespace WebCore {

using namespace KJS;

const ClassInfo JSDOMExceptionConstructor::info = { "DOMExceptionConstructor", 0, 0, 0 };

/* Source for DOMExceptionConstructorTable. Use "make hashtables" to regenerate.
@begin JSDOMExceptionConstructorTable 15
  INDEX_SIZE_ERR                WebCore::INDEX_SIZE_ERR               DontDelete|ReadOnly
  DOMSTRING_SIZE_ERR            WebCore::DOMSTRING_SIZE_ERR   DontDelete|ReadOnly
  HIERARCHY_REQUEST_ERR         WebCore::HIERARCHY_REQUEST_ERR        DontDelete|ReadOnly
  WRONG_DOCUMENT_ERR            WebCore::WRONG_DOCUMENT_ERR   DontDelete|ReadOnly
  INVALID_CHARACTER_ERR         WebCore::INVALID_CHARACTER_ERR        DontDelete|ReadOnly
  NO_DATA_ALLOWED_ERR           WebCore::NO_DATA_ALLOWED_ERR  DontDelete|ReadOnly
  NO_MODIFICATION_ALLOWED_ERR   WebCore::NO_MODIFICATION_ALLOWED_ERR  DontDelete|ReadOnly
  NOT_FOUND_ERR                 WebCore::NOT_FOUND_ERR                DontDelete|ReadOnly
  NOT_SUPPORTED_ERR             WebCore::NOT_SUPPORTED_ERR    DontDelete|ReadOnly
  INUSE_ATTRIBUTE_ERR           WebCore::INUSE_ATTRIBUTE_ERR  DontDelete|ReadOnly
  INVALID_STATE_ERR             WebCore::INVALID_STATE_ERR    DontDelete|ReadOnly
  SYNTAX_ERR                    WebCore::SYNTAX_ERR           DontDelete|ReadOnly
  INVALID_MODIFICATION_ERR      WebCore::INVALID_MODIFICATION_ERR     DontDelete|ReadOnly
  NAMESPACE_ERR                 WebCore::NAMESPACE_ERR                DontDelete|ReadOnly
  INVALID_ACCESS_ERR            WebCore::INVALID_ACCESS_ERR   DontDelete|ReadOnly
@end
*/

JSDOMExceptionConstructor::JSDOMExceptionConstructor(ExecState* exec) 
{ 
    setPrototype(exec->lexicalInterpreter()->builtinObjectPrototype());
}

bool JSDOMExceptionConstructor::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSDOMExceptionConstructor, DOMObject>(exec, &JSDOMExceptionConstructorTable, this, propertyName, slot);
}

JSValue* JSDOMExceptionConstructor::getValueProperty(ExecState*, int token) const
{
    // We use the token as the value to return directly
    return jsNumber(token);
}

JSObject* getDOMExceptionConstructor(ExecState* exec)
{
    return cacheGlobalObject<JSDOMExceptionConstructor>(exec, "[[DOMException.constructor]]");
}

} // namespace WebCore
