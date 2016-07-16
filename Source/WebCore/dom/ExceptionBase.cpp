/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExceptionBase.h"

#include "ExceptionCodeDescription.h"

namespace WebCore {

ExceptionBase::ExceptionBase(const ExceptionCodeDescription& description, MessageSource messageSource)
    : m_code(description.code)
    , m_name(description.name)
    , m_description(description.description)
    , m_typeName(description.typeName)
    , m_messageSource(messageSource)
{
    if (messageSource == MessageSource::UseDescription) {
        m_message = m_description;
        return;
    }

    if (description.name)
        m_message = m_name + ": " + description.typeName + " Exception " + String::number(description.code);
    else
        m_message = makeString(description.typeName, " Exception ", String::number(description.code));
}

String ExceptionBase::consoleErrorMessage() const
{
    if (m_messageSource == MessageSource::UseDescription)
        return toString();

    return makeString(m_message, ": ", m_description);
}

String ExceptionBase::toString() const
{
    if (m_messageSource != MessageSource::UseDescription)
        return makeString("Error: ", m_message);

    String lastComponent;
    if (!m_description.isEmpty())
        lastComponent = makeString(": ", m_description);

    if (m_name.isEmpty())
        return makeString(m_typeName, " Exception", m_code ? makeString(" ", String::number(m_code)) : "", lastComponent);

    return makeString(m_name, " (", m_typeName, " Exception", m_code ? makeString(" ", String::number(m_code)) : "", ")", lastComponent);
}

} // namespace WebCore
