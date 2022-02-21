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

#include "config.h"
#include "FragmentDirectiveParser.h"

#include "Logging.h"
#include <wtf/Deque.h>
#include <wtf/URL.h>
#include <wtf/text/TextStream.h>


namespace WebCore {

FragmentDirectiveParser::FragmentDirectiveParser(const URL& url)
{
    ASCIILiteral fragmentDirectiveDelimiter = ":~:"_s;
    auto fragmentIdentifier = url.fragmentIdentifier();
    
    if (fragmentIdentifier.isEmpty()) {
        m_remainingURLFragment = fragmentIdentifier;
        return;
    }
        
    auto fragmentDirectiveStart = fragmentIdentifier.find(StringView(fragmentDirectiveDelimiter));
    
    if (fragmentDirectiveStart == notFound) {
        m_remainingURLFragment = fragmentIdentifier;
        return;
    }
    
    auto fragmentDirective = fragmentIdentifier.substring(fragmentDirectiveStart + fragmentDirectiveDelimiter.length());
    
    // FIXME: this needs to be set on the original URL
    m_remainingURLFragment = fragmentIdentifier.left(fragmentDirectiveStart);
    
    parseFragmentDirective(fragmentDirective);
    
    m_fragmentDirective = fragmentDirective;
    m_isValid = true;
}

void FragmentDirectiveParser::parseFragmentDirective(StringView fragmentDirective)
{
    LOG_WITH_STREAM(TextFragment, stream << " parseFragmentDirective: ");
    
    Vector<ParsedTextDirective> parsedTextDirectives;
    String textDirectivePrefix = "text="_s;

    auto directives = fragmentDirective.split('&');
    
    LOG_WITH_STREAM(TextFragment, stream << " parseFragmentDirective: ");
    
    for (auto directive : directives) {
        if (!directive.startsWith(textDirectivePrefix))
            continue;
        
        auto textDirective = directive.substring(textDirectivePrefix.length());
        
        Deque<String> tokens;
        bool containsEmptyToken = false;
        for (auto token : textDirective.split(',')) {
            if (token.isEmpty()) {
                LOG_WITH_STREAM(TextFragment, stream << " empty token ");
                containsEmptyToken = true;
                break;
            }
            tokens.append(token.toString());
        }
        if (containsEmptyToken)
            continue;
        // FIXME: add decoding for % encoded strings.
        if (tokens.size() > 4 || tokens.size() < 1) {
            LOG_WITH_STREAM(TextFragment, stream << " wrong number of tokens ");
            continue;
        }
        
        ParsedTextDirective parsedTextDirective;
        
        if (tokens.first().endsWith('-') && tokens.first().length() > 1) {
            tokens.first().truncate(tokens.first().length() - 2);
            parsedTextDirective.prefix = tokens.first();
            tokens.removeFirst();
        }
        
        if (tokens.last().startsWith('-') && tokens.last().length() > 1) {
            tokens.last().remove(0);
            parsedTextDirective.suffix = tokens.takeLast();
        }
        
        if (tokens.size() != 1 && tokens.size() != 2) {
            LOG_WITH_STREAM(TextFragment, stream << " not enough tokens ");
            continue;
        }
        
        parsedTextDirective.textStart = tokens.first();
        
        if (tokens.size() == 2)
            parsedTextDirective.textEnd = tokens.last();
        
        parsedTextDirectives.append(parsedTextDirective);
    }
    
    m_parsedTextDirectives = parsedTextDirectives;
}

} // namespace WebCore
