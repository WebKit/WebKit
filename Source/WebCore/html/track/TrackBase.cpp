/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserverInlines.h"
#include "Document.h"
#include "Logging.h"
#include "TrackListBase.h"
#include "TrackPrivateBase.h"
#include "TrackPrivateBaseClient.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/Language.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

#if ENABLE(VIDEO)

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TrackBase);
WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaTrackBase);

static int s_uniqueId = 0;

static bool isValidBCP47LanguageTag(const String&);

#if !RELEASE_LOG_DISABLED
static Ref<Logger> nullLogger(TrackBase& track)
{
    static NeverDestroyed<Ref<Logger>> logger = [&] {
        Ref logger = Logger::create(&track);
        logger->setEnabled(&track, false);
        return logger;
    }();
    return logger.get();
}
#endif

TrackBase::TrackBase(ScriptExecutionContext* context, Type type, const std::optional<AtomString>& id, TrackID trackId, const AtomString& label, const AtomString& language)
    : ContextDestructionObserver(context)
    , m_uniqueId(++s_uniqueId)
    , m_id(id ? *id : AtomString::number(trackId))
    , m_trackId(trackId)
    , m_label(label)
    , m_language(language)
{
    ASSERT(type != BaseTrack);
    if (isValidBCP47LanguageTag(language))
        m_validBCP47Language = language;

    m_type = type;

#if !RELEASE_LOG_DISABLED
    m_logger = nullLogger(*this);
#endif
}

TrackBase::~TrackBase() = default;

void TrackBase::didMoveToNewDocument(Document& newDocument)
{
    observeContext(newDocument.protectedContextDocument().ptr());
}

void TrackBase::setTrackList(TrackListBase& trackList)
{
    m_trackList = trackList;
}

void TrackBase::clearTrackList()
{
    m_trackList = nullptr;
}

TrackListBase* TrackBase::trackList() const
{
    return m_trackList.get();
}

WebCoreOpaqueRoot TrackBase::opaqueRoot()
{
    // Runs on GC thread.
    if (SUPPRESS_UNCOUNTED_LOCAL auto* trackList = this->trackList())
        return trackList->opaqueRoot();
    return WebCoreOpaqueRoot { this };
}

// See: https://tools.ietf.org/html/bcp47#section-2.1
static bool isValidBCP47LanguageTag(const String& languageTag)
{
    auto const length = languageTag.length();

    // Max length picked as double the longest example tag in spec which is 49 characters:
    // https://tools.ietf.org/html/bcp47#section-4.4.2
    if (length < 2 || length > 100)
        return false;

    char16_t firstChar = languageTag[0];

    if (!isASCIIAlpha(firstChar))
        return false;

    char16_t secondChar = languageTag[1];

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
        char16_t c = languageTag[nextCharIndexToCheck];
        if (isASCIIAlphanumeric(c) || c == '-')
            continue;
        return false;
    }
    return true;
}
    
void TrackBase::setLanguage(const AtomString& language)
{
    m_language = language;
    if (language.isEmpty() || isValidBCP47LanguageTag(language)) {
        m_validBCP47Language = language;
        return;
    }

    m_validBCP47Language = emptyAtom();

    RefPtr context = scriptExecutionContext();
    if (!context)
        return;

    String message;
    if (language.contains((char16_t)'\0'))
        message = "The language contains a null character and is not a valid BCP 47 language tag."_s;
    else
        message = makeString("The language '"_s, language, "' is not a valid BCP 47 language tag."_s);

    context->addConsoleMessage(MessageSource::Rendering, MessageLevel::Warning, message);
}

#if !RELEASE_LOG_DISABLED
void TrackBase::setLogger(const Logger& logger, uint64_t logIdentifier)
{
    m_logger = logger;
    m_logIdentifier = childLogIdentifier(logIdentifier, m_uniqueId);
}

WTFLogChannel& TrackBase::logChannel() const
{
    return LogMedia;
}
#endif

void TrackBase::addClientToTrackPrivateBase(TrackPrivateBaseClient& client, TrackPrivateBase& track)
{
    if (RefPtr context = scriptExecutionContext()) {
        m_clientRegistrationId = track.addClient([contextIdentifier = context->identifier()](auto&& task) {
            ScriptExecutionContext::ensureOnContextThread(contextIdentifier, WTFMove(task));
        }, client);
    }
}

void TrackBase::removeClientFromTrackPrivateBase(TrackPrivateBase& track)
{
    track.removeClient(m_clientRegistrationId);
}

static std::optional<AtomString> trackUID(const std::optional<String>& id)
{
    if (!id)
        return { };
    return AtomString { *id };
}

MediaTrackBase::MediaTrackBase(ScriptExecutionContext* context, Type type, const std::optional<String>& id, TrackID trackId, const String& label, const String& language)
    : TrackBase(context, type, trackUID(id), trackId, AtomString { label.isolatedCopy() }, AtomString { language.isolatedCopy() })
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
