/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "kjs_binding.h"
#import "kjs_dom.h"
#import "DOM.h"
#import "DOMInternal.h"

#import <JavaScriptCore/WebScriptObjectPrivate.h>

// This file makes use of the ObjC DOM API, and the C++ DOM API, so we need to be careful about what
// headers are included to avoid naming conflicts.

inline id createObjCDOMNode(DOM::NodeImpl *node)
{
    return [DOMNode _nodeWithImpl:node];
}

namespace KJS {

void *ScriptInterpreter::createObjcInstanceForValue (ExecState *exec, const Object &value, const Bindings::RootObject *origin, const Bindings::RootObject *current)
{
    if (value.inherits(&DOMNode::info)) {
	DOMNode *imp = static_cast<DOMNode *>(value.imp());
	DOM::Node node = imp->toNode();

	id newObjcNode = createObjCDOMNode(node.handle());
	ObjectImp *scriptImp = static_cast<ObjectImp *>(getDOMNode(exec, node).imp());

	[newObjcNode _initializeWithObjectImp:scriptImp originExecutionContext:origin executionContext:current];
	
	return newObjcNode;
    }
    return 0;
}

}
