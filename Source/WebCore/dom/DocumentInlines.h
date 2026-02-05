/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/ClientOrigin.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentParser.h>
#include <WebCore/DocumentSyncData.h>
#include <WebCore/Element.h>
#include <WebCore/ExtensionStyleSheets.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/NodeIterator.h>
#include <WebCore/ReportingScope.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/TextResourceDecoder.h>
#include <WebCore/UndoManager.h>

namespace WebCore {

inline PAL::TextEncoding Document::textEncoding() const
{
    if (RefPtr decoder = this->decoder())
        return decoder->encoding();
    return PAL::TextEncoding();
}

inline ASCIILiteral Document::encoding() const
{
    return textEncoding().domName();
}

inline ASCIILiteral Document::charset() const
{
    return Document::encoding();
}

inline ExtensionStyleSheets& Document::extensionStyleSheets()
{
    if (!m_extensionStyleSheets)
        return ensureExtensionStyleSheets();
    return *m_extensionStyleSheets;
}

inline CheckedRef<ExtensionStyleSheets> Document::checkedExtensionStyleSheets()
{
    return extensionStyleSheets();
}

inline VisitedLinkState& Document::visitedLinkState() const
{
    if (!m_visitedLinkState)
        return const_cast<Document&>(*this).ensureVisitedLinkState();
    return *m_visitedLinkState;
}

inline ScriptRunner& Document::scriptRunner()
{
    if (!m_scriptRunner)
        return ensureScriptRunner();
    return *m_scriptRunner;
}

inline ScriptModuleLoader& Document::moduleLoader()
{
    if (!m_moduleLoader)
        return ensureModuleLoader();
    return *m_moduleLoader;
}

inline CSSFontSelector& Document::fontSelector() const
{
    if (!m_fontSelector)
        return const_cast<Document&>(*this).ensureFontSelector();
    return *m_fontSelector;
}

inline const Document* Document::templateDocument() const
{
    return m_templateDocumentHost ? this : m_templateDocument.get();
}

inline Ref<Document> Document::create(const Settings& settings, const URL& url)
{
    auto document = adoptRef(*new Document(nullptr, settings, url));
    document->addToContextsMap();
    return document;
}

bool Document::hasNodeIterators() const
{
    return !m_nodeIterators.isEmptyIgnoringNullReferences();
}

inline void Document::invalidateAccessKeyCache()
{
    if (m_accessKeyCache) [[unlikely]]
        invalidateAccessKeyCacheSlowCase();
}

inline ClientOrigin Document::clientOrigin() const { return { topOrigin().data(), securityOrigin().data() }; }


inline bool Document::wasLastFocusByClick() const { return m_latestFocusTrigger == FocusTrigger::Click; }

inline UndoManager& Document::undoManager() const
{
    if (!m_undoManager)
        return const_cast<Document&>(*this).ensureUndoManager();
    return *m_undoManager;
}

inline ReportingScope& Document::reportingScope() const
{
    if (!m_reportingScope)
        return const_cast<Document&>(*this).ensureReportingScope();
    return *m_reportingScope;
}

inline Ref<DocumentSyncData> Document::syncData()
{
    return m_syncData.get();
}

// FIXME: Move to FrameSelectionInlines.h
RefPtr<Document> FrameSelection::protectedDocument() const
{
    return m_document.get();
}

} // namespace WebCore
