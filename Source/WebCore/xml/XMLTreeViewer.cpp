/*
 * Copyright (C) 2011-2014 Google Inc. All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "XMLTreeViewer.h"

#if ENABLE(XSLT)

#include "Document.h"
#include "Element.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "ScriptController.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "StyleScope.h"
#include "Text.h"
#include "XMLViewerCSS.h"
#include "XMLViewerJS.h"

namespace WebCore {

XMLTreeViewer::XMLTreeViewer(Document& document)
    : m_document(document)
{
}

void XMLTreeViewer::transformDocumentToTreeView()
{
    String scriptString = StringImpl::createWithoutCopying(XMLViewer_js, sizeof(XMLViewer_js));
    m_document.frame()->script().evaluateIgnoringException(ScriptSourceCode(scriptString, JSC::SourceTaintedOrigin::Untainted));
    m_document.frame()->script().evaluateIgnoringException(ScriptSourceCode(AtomString("prepareWebKitXMLViewer('This XML file does not appear to have any style information associated with it. The document tree is shown below.');"_s), JSC::SourceTaintedOrigin::Untainted));

    String cssString = StringImpl::createWithoutCopying(XMLViewer_css, sizeof(XMLViewer_css));
    auto text = m_document.createTextNode(WTFMove(cssString));
    m_document.getElementById(String("xml-viewer-style"_s))->appendChild(text);
}

} // namespace WebCore

#endif // ENABLE(XSLT)
