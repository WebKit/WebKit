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

#include "FragmentDirectiveUtilities.h"
#include "Logging.h"
#include <wtf/Deque.h>
#include <wtf/URL.h>
#include <wtf/URLParser.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FragmentDirectiveParser::FragmentDirectiveParser(StringView fragmentDirective)
{
    parseFragmentDirective(fragmentDirective);
    
    m_fragmentDirective = fragmentDirective;
    m_isValid = true;
}

FragmentDirectiveParser::~FragmentDirectiveParser() = default;

// https://wicg.github.io/scroll-to-text-fragment/#parse-a-text-directive
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
        if (tokens.size() > 4 || tokens.size() < 1) {
            LOG_WITH_STREAM(TextFragment, stream << " wrong number of tokens ");
            continue;
        }
        
        ParsedTextDirective parsedTextDirective;
        
        if (tokens.first().endsWith('-') && tokens.first().length() > 1) {
            auto takenFirstToken = tokens.takeFirst();
            if (auto prefix = WTF::URLParser::formURLDecode(StringView(takenFirstToken).left(takenFirstToken.length() - 1)))
                parsedTextDirective.prefix = WTFMove(*prefix);
            else
                LOG_WITH_STREAM(TextFragment, stream << " could not decode prefix ");
        }
        
        if (tokens.isEmpty()) {
            LOG_WITH_STREAM(TextFragment, stream << " not enough tokens ");
            continue;
        }

        if (tokens.last().startsWith('-') && tokens.last().length() > 1) {
            tokens.last() = tokens.last().substring(1);
            
            if (auto suffix = WTF::URLParser::formURLDecode(tokens.takeLast()))
                parsedTextDirective.suffix = WTFMove(*suffix);
            else
                LOG_WITH_STREAM(TextFragment, stream << " could not decode suffix ");
        }
        
        if (tokens.size() != 1 && tokens.size() != 2) {
            LOG_WITH_STREAM(TextFragment, stream << " not enough tokens ");
            continue;
        }
        
        if (auto start = WTF::URLParser::formURLDecode(tokens.first()))
            parsedTextDirective.startText = WTFMove(*start);
        else
            LOG_WITH_STREAM(TextFragment, stream << " could not decode start ");
        
        if (tokens.size() == 2) {
            if (auto end = WTF::URLParser::formURLDecode(tokens.last()))
                parsedTextDirective.endText = WTFMove(*end);
            else
                LOG_WITH_STREAM(TextFragment, stream << " could not decode end ");
        }
        
        parsedTextDirectives.append(parsedTextDirective);
    }
    
    m_parsedTextDirectives = parsedTextDirectives;
}

} // namespace WebCore
