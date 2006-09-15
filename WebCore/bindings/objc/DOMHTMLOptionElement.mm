/*
 * Copyright (C) 2004-2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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

#import "config.h"
#import "DOMHTMLOptionElement.h"

#import "DOMInternal.h"
#import "HTMLOptionElement.h"
#import "DOMHTMLFormElement.h"
#import "PlatformString.h"


// FIXME: DOMHTMLOptionElement cannot be auto-generated yet because of some 
// differeneces between the old bindings and the idl.
@implementation DOMHTMLOptionElement

#define IMPL static_cast<WebCore::HTMLOptionElement*>(reinterpret_cast<WebCore::Node*>(_internal))

- (DOMHTMLFormElement *)form
{
    return [DOMHTMLFormElement _HTMLFormElementWith:IMPL->form()];
}

- (BOOL)defaultSelected
{
    return IMPL->defaultSelected();
}

- (void)setDefaultSelected:(BOOL)defaultSelected
{
    IMPL->setDefaultSelected(defaultSelected);
}

- (NSString *)text
{
    return IMPL->text();
}

- (int)index
{
    return IMPL->index();
}

- (BOOL)disabled
{
    return IMPL->disabled();
}

- (void)setDisabled:(BOOL)disabled
{
    IMPL->setDisabled(disabled);
}

- (NSString *)label
{
    return IMPL->label();
}

- (void)setLabel:(NSString *)label
{
    IMPL->setLabel(label);
}

- (BOOL)selected
{
    return IMPL->selected();
}

- (void)setSelected:(BOOL)selected
{
    IMPL->setSelected(selected);
}

- (NSString *)value
{
    return IMPL->value();
}

- (void)setValue:(NSString *)value
{
    IMPL->setValue(value);
}

@end

@implementation DOMHTMLOptionElement (WebCoreInternal)

- (WebCore::HTMLOptionElement *)_HTMLOptionElement
{
    return IMPL;
}

+ (DOMHTMLOptionElement *)_HTMLOptionElementWith:(WebCore::HTMLOptionElement *)impl
{
    return static_cast<DOMHTMLOptionElement*>([DOMNode _nodeWith:impl]);
}

@end
