/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#ifndef __remove_css_property_command_h__
#define __remove_css_property_command_h__

#include "EditCommand.h"

#include "PlatformString.h"

namespace WebCore {
    class CSSStyleDeclaration;
}

namespace WebCore {

class RemoveCSSPropertyCommand : public EditCommand
{
public:
    RemoveCSSPropertyCommand(WebCore::Document *, WebCore::CSSStyleDeclaration *, int property);
    virtual ~RemoveCSSPropertyCommand() { }

    virtual void doApply();
    virtual void doUnapply();

    WebCore::CSSMutableStyleDeclaration *styleDeclaration() const { return m_decl.get(); }
    int property() const { return m_property; }
    
private:
    RefPtr<WebCore::CSSMutableStyleDeclaration> m_decl;
    int m_property;
    WebCore::String m_oldValue;
    bool m_important;
};

} // namespace WebCore

#endif // __remove_css_property_command_h__
