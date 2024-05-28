/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ListStyleType.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSValueKeywords.h"

namespace WebCore {

bool ListStyleType::isCircle() const
{
    return type == Type::CounterStyle && identifier == nameLiteral(CSSValueCircle);
}

bool ListStyleType::isSquare() const
{
    return type == Type::CounterStyle && identifier == nameLiteral(CSSValueSquare);
}

bool ListStyleType::isDisc() const
{
    return type == Type::CounterStyle && identifier == nameLiteral(CSSValueDisc);
}

TextStream& operator<<(TextStream& ts, ListStyleType::Type styleType)
{
    return ts << nameLiteral(toCSSValueID(styleType)).characters();
}

WTF::TextStream& operator<<(WTF::TextStream& ts, ListStyleType listStyle)
{
    if (listStyle.type == ListStyleType::Type::CounterStyle)
        ts << listStyle.identifier;
    else if (listStyle.type == ListStyleType::Type::String)
        ts << "\"" << listStyle.identifier << "\"";
    else
        ts << listStyle.type;
    return ts;
}

} // namespace WebCore
