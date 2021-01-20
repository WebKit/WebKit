/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "FontFaceSet.h"

#include "DOMPromiseProxy.h"
#include "Document.h"
#include "EventLoop.h"
#include "FontFace.h"
#include "FrameLoader.h"
#include "JSDOMBinding.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFontFace.h"
#include "JSFontFaceSet.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(FontFaceSet);

Ref<FontFaceSet> FontFaceSet::create(Document& document, const Vector<RefPtr<FontFace>>& initialFaces)
{
    Ref<FontFaceSet> result = adoptRef(*new FontFaceSet(document, initialFaces));
    result->suspendIfNeeded();
    return result;
}

Ref<FontFaceSet> FontFaceSet::create(Document& document, CSSFontFaceSet& backing)
{
    Ref<FontFaceSet> result = adoptRef(*new FontFaceSet(document, backing));
    result->suspendIfNeeded();
    return result;
}

FontFaceSet::FontFaceSet(Document& document, const Vector<RefPtr<FontFace>>& initialFaces)
    : ActiveDOMObject(document)
    , m_backing(CSSFontFaceSet::create())
    , m_readyPromise(makeUniqueRef<ReadyPromise>(*this, &FontFaceSet::readyPromiseResolve))
{
    m_backing->addClient(*this);
    for (auto& face : initialFaces)
        add(*face);
}

FontFaceSet::FontFaceSet(Document& document, CSSFontFaceSet& backing)
    : ActiveDOMObject(document)
    , m_backing(backing)
    , m_readyPromise(makeUniqueRef<ReadyPromise>(*this, &FontFaceSet::readyPromiseResolve))
{
    if (document.frame())
        m_isDocumentLoaded = document.loadEventFinished() && !document.processingLoadEvent();

    if (m_isDocumentLoaded && !backing.hasActiveFontFaces())
        m_readyPromise->resolve(*this);

    m_backing->addClient(*this);
}

FontFaceSet::~FontFaceSet()
{
    m_backing->removeClient(*this);
}

FontFaceSet::Iterator::Iterator(FontFaceSet& set)
    : m_target(set)
{
}

RefPtr<FontFace> FontFaceSet::Iterator::next()
{
    if (m_index == m_target->size())
        return nullptr;
    return m_target->backing()[m_index++].wrapper();
}

FontFaceSet::PendingPromise::PendingPromise(LoadPromise&& promise)
    : promise(makeUniqueRef<LoadPromise>(WTFMove(promise)))
{
}

FontFaceSet::PendingPromise::~PendingPromise() = default;

bool FontFaceSet::has(FontFace& face) const
{
    return m_backing->hasFace(face.backing());
}

size_t FontFaceSet::size() const
{
    return m_backing->faceCount();
}

FontFaceSet& FontFaceSet::add(FontFace& face)
{
    if (!m_backing->hasFace(face.backing()))
        m_backing->add(face.backing());
    return *this;
}

bool FontFaceSet::remove(FontFace& face)
{
    bool result = m_backing->hasFace(face.backing());
    if (result)
        m_backing->remove(face.backing());
    return result;
}

void FontFaceSet::clear()
{
    while (m_backing->faceCount())
        m_backing->remove(m_backing.get()[0]);
}

void FontFaceSet::load(const String& font, const String& text, LoadPromise&& promise)
{
    auto matchingFacesResult = m_backing->matchingFacesExcludingPreinstalledFonts(font, text);
    if (matchingFacesResult.hasException()) {
        promise.reject(matchingFacesResult.releaseException());
        return;
    }
    auto matchingFaces = matchingFacesResult.releaseReturnValue();

    if (matchingFaces.isEmpty()) {
        promise.resolve({ });
        return;
    }

    for (auto& face : matchingFaces)
        face.get().load();

    for (auto& face : matchingFaces) {
        if (face.get().status() == CSSFontFace::Status::Failure) {
            promise.reject(NetworkError);
            return;
        }
    }

    auto pendingPromise = PendingPromise::create(WTFMove(promise));
    bool waiting = false;

    for (auto& face : matchingFaces) {
        pendingPromise->faces.append(face.get().wrapper());
        if (face.get().status() == CSSFontFace::Status::Success)
            continue;
        waiting = true;
        ASSERT(face.get().existingWrapper());
        m_pendingPromises.add(face.get().existingWrapper(), Vector<Ref<PendingPromise>>()).iterator->value.append(pendingPromise.copyRef());
    }

    if (!waiting)
        pendingPromise->promise->resolve(pendingPromise->faces);
}

ExceptionOr<bool> FontFaceSet::check(const String& family, const String& text)
{
    return m_backing->check(family, text);
}
    
auto FontFaceSet::status() const -> LoadStatus
{
    switch (m_backing->status()) {
    case CSSFontFaceSet::Status::Loading:
        return LoadStatus::Loading;
    case CSSFontFaceSet::Status::Loaded:
        return LoadStatus::Loaded;
    }
    ASSERT_NOT_REACHED();
    return LoadStatus::Loaded;
}

void FontFaceSet::startedLoading()
{
    // FIXME: Fire a "loading" event asynchronously.
}

void FontFaceSet::documentDidFinishLoading()
{
    m_isDocumentLoaded = true;
    if (!m_backing->hasActiveFontFaces() && !m_readyPromise->isFulfilled())
        m_readyPromise->resolve(*this);
}

void FontFaceSet::completedLoading()
{
    if (m_isDocumentLoaded && !m_readyPromise->isFulfilled())
        m_readyPromise->resolve(*this);
}

void FontFaceSet::faceFinished(CSSFontFace& face, CSSFontFace::Status newStatus)
{
    if (!face.existingWrapper())
        return;

    auto pendingPromises = m_pendingPromises.take(face.existingWrapper());
    if (pendingPromises.isEmpty())
        return;

    for (auto& pendingPromise : pendingPromises) {
        if (pendingPromise->hasReachedTerminalState)
            continue;
        if (newStatus == CSSFontFace::Status::Success) {
            if (pendingPromise->hasOneRef()) {
                pendingPromise->promise->resolve(pendingPromise->faces);
                pendingPromise->hasReachedTerminalState = true;
            }
        } else {
            ASSERT(newStatus == CSSFontFace::Status::Failure);
            pendingPromise->promise->reject(NetworkError);
            pendingPromise->hasReachedTerminalState = true;
        }
    }
}

FontFaceSet& FontFaceSet::readyPromiseResolve()
{
    return *this;
}

}
