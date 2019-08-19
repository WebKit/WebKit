/*
 * Copyright (C) 2000 Peter Kelly (pmk@post.com)
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "XMLDocumentParser.h"

#include "CDATASection.h"
#include "Comment.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "HTMLStyleElement.h"
#include "ImageLoader.h"
#include "PendingScript.h"
#include "ProcessingInstruction.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SVGNames.h"
#include "SVGStyleElement.h"
#include "ScriptElement.h"
#include "ScriptSourceCode.h"
#include "StyleScope.h"
#include "TextResourceDecoder.h"
#include "TreeDepthLimit.h"
#include <wtf/Ref.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace HTMLNames;

void XMLDocumentParser::pushCurrentNode(ContainerNode* n)
{
    ASSERT(n);
    ASSERT(m_currentNode);
    if (n != document())
        n->ref();
    m_currentNodeStack.append(m_currentNode);
    m_currentNode = n;
    if (m_currentNodeStack.size() > maxDOMTreeDepth)
        handleError(XMLErrors::fatal, "Excessive node nesting.", textPosition());
}

void XMLDocumentParser::popCurrentNode()
{
    if (!m_currentNode)
        return;
    ASSERT(m_currentNodeStack.size());

    if (m_currentNode != document())
        m_currentNode->deref();

    m_currentNode = m_currentNodeStack.last();
    m_currentNodeStack.removeLast();
}

void XMLDocumentParser::clearCurrentNodeStack()
{
    if (m_currentNode && m_currentNode != document())
        m_currentNode->deref();
    m_currentNode = nullptr;
    m_leafTextNode = nullptr;

    if (m_currentNodeStack.size()) { // Aborted parsing.
        for (size_t i = m_currentNodeStack.size() - 1; i != 0; --i)
            m_currentNodeStack[i]->deref();
        if (m_currentNodeStack[0] && m_currentNodeStack[0] != document())
            m_currentNodeStack[0]->deref();
        m_currentNodeStack.clear();
    }
}

void XMLDocumentParser::insert(SegmentedString&&)
{
    ASSERT_NOT_REACHED();
}

void XMLDocumentParser::append(RefPtr<StringImpl>&& inputSource)
{
    String source { WTFMove(inputSource) };

    if (m_sawXSLTransform || !m_sawFirstElement)
        m_originalSourceForTransform.append(source);

    if (isStopped() || m_sawXSLTransform)
        return;

    if (m_parserPaused) {
        m_pendingSrc.append(source);
        return;
    }

    doWrite(source);

    // After parsing, dispatch image beforeload events.
    ImageLoader::dispatchPendingBeforeLoadEvents();
}

void XMLDocumentParser::handleError(XMLErrors::ErrorType type, const char* m, TextPosition position)
{
    if (!m_xmlErrors)
        m_xmlErrors = makeUnique<XMLErrors>(*document());
    m_xmlErrors->handleError(type, m, position);
    if (type != XMLErrors::warning)
        m_sawError = true;
    if (type == XMLErrors::fatal)
        stopParsing();
}

void XMLDocumentParser::createLeafTextNode()
{
    if (m_leafTextNode)
        return;

    ASSERT(m_bufferedText.size() == 0);
    ASSERT(!m_leafTextNode);
    m_leafTextNode = Text::create(m_currentNode->document(), "");
    m_currentNode->parserAppendChild(*m_leafTextNode);
}

bool XMLDocumentParser::updateLeafTextNode()
{
    if (isStopped())
        return false;

    if (!m_leafTextNode)
        return true;

    // This operation might fire mutation event, see below.
    m_leafTextNode->appendData(String::fromUTF8(reinterpret_cast<const char*>(m_bufferedText.data()), m_bufferedText.size()));
    m_bufferedText = { };

    m_leafTextNode = nullptr;

    // Hence, we need to check again whether the parser is stopped, since mutation
    // event handlers executed by appendData might have detached this parser.
    return !isStopped();
}

void XMLDocumentParser::detach()
{
    clearCurrentNodeStack();
    ScriptableDocumentParser::detach();
}

void XMLDocumentParser::end()
{
    // XMLDocumentParserLibxml2 will do bad things to the document if doEnd() is called.
    // I don't believe XMLDocumentParserQt needs doEnd called in the fragment case.
    ASSERT(!m_parsingFragment);

    doEnd();

    // doEnd() call above can detach the parser and null out its document.
    // In that case, we just bail out.
    if (isDetached())
        return;

    // doEnd() could process a script tag, thus pausing parsing.
    if (m_parserPaused)
        return;

    if (m_sawError && !isStopped()) {
        insertErrorMessageBlock();
        if (isDetached()) // Inserting an error message may have ran arbitrary scripts.
            return;
    } else {
        updateLeafTextNode();
        document()->styleScope().didChangeStyleSheetEnvironment();
    }

    if (isParsing())
        prepareToStopParsing();
    document()->setReadyState(Document::Interactive);
    clearCurrentNodeStack();
    document()->finishedParsing();
}

void XMLDocumentParser::finish()
{
    // FIXME: We should ASSERT(!m_parserStopped) here, since it does not
    // makes sense to call any methods on DocumentParser once it's been stopped.
    // However, FrameLoader::stop calls DocumentParser::finish unconditionally.

    Ref<XMLDocumentParser> protectedThis(*this);

    if (m_parserPaused)
        m_finishCalled = true;
    else
        end();
}

void XMLDocumentParser::insertErrorMessageBlock()
{
    ASSERT(m_xmlErrors);
    m_xmlErrors->insertErrorMessageBlock();
}

void XMLDocumentParser::notifyFinished(PendingScript& pendingScript)
{
    ASSERT(&pendingScript == m_pendingScript.get());

    // JavaScript can detach this parser, make sure it's kept alive even if detached.
    Ref<XMLDocumentParser> protectedThis(*this);

    m_pendingScript = nullptr;
    pendingScript.clearClient();

    pendingScript.element().executePendingScript(pendingScript);

    if (!isDetached() && !m_requestingScript)
        resumeParsing();
}

bool XMLDocumentParser::isWaitingForScripts() const
{
    return m_pendingScript;
}

void XMLDocumentParser::pauseParsing()
{
    ASSERT(!m_parserPaused);

    if (m_parsingFragment)
        return;

    m_parserPaused = true;
}

bool XMLDocumentParser::parseDocumentFragment(const String& chunk, DocumentFragment& fragment, Element* contextElement, ParserContentPolicy parserContentPolicy)
{
    if (!chunk.length())
        return true;

    // FIXME: We need to implement the HTML5 XML Fragment parsing algorithm:
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-xhtml-syntax.html#xml-fragment-parsing-algorithm
    // For now we have a hack for script/style innerHTML support:
    if (contextElement && (contextElement->hasLocalName(HTMLNames::scriptTag->localName()) || contextElement->hasLocalName(HTMLNames::styleTag->localName()))) {
        fragment.parserAppendChild(fragment.document().createTextNode(chunk));
        return true;
    }

    auto parser = XMLDocumentParser::create(fragment, contextElement, parserContentPolicy);
    bool wellFormed = parser->appendFragmentSource(chunk);
    // Do not call finish(). The finish() and doEnd() implementations touch the main document and loader and can cause crashes in the fragment case.
    parser->detach(); // Allows ~DocumentParser to assert it was detached before destruction.
    return wellFormed; // appendFragmentSource()'s wellFormed is more permissive than Document::wellFormed().
}

} // namespace WebCore
