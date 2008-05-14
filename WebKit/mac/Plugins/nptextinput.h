/*
 * Copyright (C) 2008, Apple Inc. and The Mozilla Foundation. 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the names of Apple Inc. ("Apple") or The Mozilla
 * Foundation ("Mozilla") nor the names of their contributors may be used
 * to endorse or promote products derived from this software without
 * specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE, MOZILLA AND THEIR CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE, MOZILLA OR
 * THEIR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef __OBJC__
#error "npinput.h can only be included from Objective-C code."
#endif

#ifndef _NP_TEXTINPUT_H_
#define _NP_TEXTINPUT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <WebKit/npapi.h>

#import <Cocoa/Cocoa.h>

typedef void (*NPP_InsertTextFunc)(NPP npp, id aString);
typedef void (*NPP_DoCommandBySelectorFunc)(NPP npp, SEL aSelector);
typedef void (*NPP_SetMarkedTextFunc)(NPP npp, id aString, NSRange selRange);
typedef void (*NPP_UnmarkTextFunc)(NPP npp);
typedef BOOL (*NPP_HasMarkedTextFunc)(NPP npp);
typedef NSAttributedString * (*NPP_AttributedSubstringFromRangeFunc)(NPP npp, NSRange theRange);
typedef NSRange (*NPP_MarkedRangeFunc)(NPP npp);
typedef NSRange (*NPP_SelectedRangeFunc)(NPP npp);
typedef NSRect (*NPP_FirstRectForCharacterRangeFunc)(NPP npp, NSRange theRange);
typedef unsigned long long (*NPP_CharacterIndexForPointFunc)(NPP npp, NSPoint thePoint);
typedef NSArray *(*NPP_ValidAttributesForMarkedTextFunc)(NPP npp);

typedef struct _NPPluginTextInputFuncs {
    uint16 size;
    uint16 version;
    
    NPP_InsertTextFunc insertText;
    NPP_DoCommandBySelectorFunc doCommandBySelector;
    NPP_SetMarkedTextFunc setMarkedText;
    NPP_UnmarkTextFunc unmarkText;
    NPP_HasMarkedTextFunc hasMarkedText;
    NPP_AttributedSubstringFromRangeFunc attributedSubstringFromRange;
    NPP_MarkedRangeFunc markedRange;
    NPP_SelectedRangeFunc selectedRange;
    NPP_FirstRectForCharacterRangeFunc firstRectForCharacterRange;
    NPP_CharacterIndexForPointFunc characterIndexForPoint;
    NPP_ValidAttributesForMarkedTextFunc validAttributesForMarkedText;
} NPPluginTextInputFuncs;

void NPP_InsertText(NPP npp, id aString);
void NPP_DoCommandBySelector(NPP npp, SEL aSelector);
void NPP_SetMarkedText(NPP npp, id aString, NSRange selRange);
void NPP_UnmarkText(NPP npp);
BOOL NPP_HasMarkedText(NPP npp);
NSAttributedString *NPP_AttributedSubstringFromRange(NPP npp, NSRange theRange);
NSRange NPP_MarkedRange(NPP npp);
NSRange NPP_SelectedRange(NPP npp);
NSRect NPP_FirstRectForCharacterRange(NPP npp, NSRange theRange);
unsigned long long NPP_CharacterIndexForPoint(NPP npp, NSPoint thePoint);
NSArray *NPP_ValidAttributesForMarkedText(NPP npp);

typedef void (*NPN_MarkedTextAbandonedFunc)(NPP npp);
typedef void (*NPN_MarkedTextSelectionChangedFunc)(NPP npp, NSRange newSel);

typedef struct _NPBrowserTextInputFuncs {
    uint16 size;
    uint16 version;
    
    NPN_MarkedTextAbandonedFunc markedTextAbandoned;
    NPN_MarkedTextSelectionChangedFunc markedTextSelectionChanged;
} NPBrowserTextInputFuncs;

void NPN_MarkedTextAbandoned(NPP npp);
void NPN_MarkedTextSelectionChanged(NPP npp, NSRange newSel);

#ifdef __cplusplus
}
#endif

#endif
