/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "TrackBase.h"

#include "Logging.h"
#include <wtf/Language.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(VIDEO_TRACK)

#include "HTMLMediaElement.h"

namespace WebCore {

static int s_uniqueId = 0;

#if !RELEASE_LOG_DISABLED
static const void* nextLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber();
    return reinterpret_cast<const void*>(++logIdentifier);
}

static RefPtr<Logger>& nullLogger()
{
    static NeverDestroyed<RefPtr<Logger>> logger;
    return logger;
}
#endif

TrackBase::TrackBase(Type type, const AtomString& id, const AtomString& label, const AtomString& language)
    : m_uniqueId(++s_uniqueId)
    , m_id(id)
    , m_label(label)
    , m_language(language)
    , m_validBCP47Language(language)
{
    ASSERT(type != BaseTrack);
    m_type = type;

#if !RELEASE_LOG_DISABLED
    if (!nullLogger().get()) {
        nullLogger() = Logger::create(this);
        nullLogger()->setEnabled(this, false);
    }

    m_logger = nullLogger().get();
    m_logIdentifier = nextLogIdentifier();
#endif
}

Element* TrackBase::element()
{
    return m_mediaElement.get();
}

void TrackBase::setMediaElement(WeakPtr<HTMLMediaElement> element)
{
    m_mediaElement = element;

#if !RELEASE_LOG_DISABLED
    if (element) {
        m_logger = &element->logger();
        m_logIdentifier = element->logIdentifier();
    }
#endif
}

// See: https://tools.ietf.org/html/bcp47#section-2.1
static bool isValidBCP47LanguageTag(const String& languageTag)
{
    auto const length = languageTag.length();

    // Max length picked as double the longest example tag in spec which is 49 characters:
    // https://tools.ietf.org/html/bcp47#section-4.4.2
    if (length < 2 || length > 100)
        return false;

    UChar firstChar = languageTag[0];

    if (!isASCIIAlpha(firstChar))
        return false;

    UChar secondChar = languageTag[1];

    if (length == 2)
        return isASCIIAlpha(secondChar);

    bool grandFatheredIrregularOrPrivateUse = (firstChar == 'i' || firstChar == 'x') && secondChar == '-';
    unsigned nextCharIndexToCheck;

    if (!grandFatheredIrregularOrPrivateUse) {
        if (!isASCIIAlpha(secondChar))
            return false;

        if (length == 3)
            return isASCIIAlpha(languageTag[2]);

        if (isASCIIAlpha(languageTag[2])) {
            if (languageTag[3] == '-')
                nextCharIndexToCheck = 4;
            else
                return false;
        } else if (languageTag[2] == '-')
            nextCharIndexToCheck = 3;
        else
            return false;
    } else
        nextCharIndexToCheck = 2;

    for (; nextCharIndexToCheck < length; ++nextCharIndexToCheck) {
        UChar c = languageTag[nextCharIndexToCheck];
        if (isASCIIAlphanumeric(c) || c == '-')
            continue;
        return false;
    }
    return true;
}
    
void TrackBase::setLanguage(const AtomString& language)
{
    if (!language.isEmpty() && !isValidBCP47LanguageTag(language)) {
        String message;
        if (language.contains((UChar)'\0'))
            message = "The language contains a null character and is not a valid BCP 47 language tag."_s;
        else {
            StringBuilder stringBuilder;
            stringBuilder.appendLiteral("The language '");
            stringBuilder.append(language);
            stringBuilder.appendLiteral("' is not a valid BCP 47 language tag.");
            message = stringBuilder.toString();
        }
        if (auto element = this->element())
            element->document().addConsoleMessage(MessageSource::Rendering, MessageLevel::Warning, message);
    } else
        m_validBCP47Language = language;
    
    m_language = language;
}

AtomString TrackBase::validBCP47Language() const
{
    return m_validBCP47Language;
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& TrackBase::logChannel() const
{
    return LogMedia;
}
#endif

MediaTrackBase::MediaTrackBase(Type type, const AtomString& id, const AtomString& label, const AtomString& language)
    : TrackBase(type, id, label, language)
{
}

void MediaTrackBase::setKind(const AtomString& kind)
{
    setKindInternal(kind);
}

void MediaTrackBase::setKindInternal(const AtomString& kind)
{
    if (isValidKind(kind))
        m_kind = kind;
    else
        m_kind = emptyAtom();
}

} // namespace WebCore

#endif
