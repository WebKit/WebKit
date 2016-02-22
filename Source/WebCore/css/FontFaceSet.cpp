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

#include "Document.h"
#include "ExceptionCodeDescription.h"
#include "FontFace.h"
#include "JSDOMBinding.h"
#include "JSDOMCoreException.h"
#include "JSFontFace.h"
#include "JSFontFaceSet.h"

namespace WebCore {

static FontFaceSet::Promise createPromise(JSC::ExecState& exec)
{
    JSDOMGlobalObject& globalObject = *JSC::jsCast<JSDOMGlobalObject*>(exec.lexicalGlobalObject());
    return FontFaceSet::Promise(DeferredWrapper(&exec, &globalObject, JSC::JSPromiseDeferred::create(&exec, &globalObject)));
}

FontFaceSet::FontFaceSet(JSC::ExecState& execState, Document& document, const Vector<RefPtr<FontFace>>& initialFaces)
    : ActiveDOMObject(&document)
    , m_backing(*this)
    , m_promise(createPromise(execState))
{
    for (auto& face : initialFaces)
        add(face.get());
}

FontFaceSet::~FontFaceSet()
{
}

FontFaceSet::Iterator::Iterator(FontFaceSet& set)
    : m_target(set)
{
}

bool FontFaceSet::Iterator::next(FontFace*& key, FontFace*& value)
{
    if (m_index == m_target->size())
        return true;
    key = m_target->m_backing[m_index++].wrapper();
    value = key;
    return false;
}

FontFaceSet::PendingPromise::PendingPromise(Promise&& promise)
    : promise(WTFMove(promise))
{
}

FontFaceSet::PendingPromise::~PendingPromise()
{
}

bool FontFaceSet::has(FontFace* face) const
{
    if (!face)
        return false;
    return m_backing.hasFace(face->backing());
}

size_t FontFaceSet::size() const
{
    return m_backing.faceCount();
}

FontFaceSet& FontFaceSet::add(FontFace* face)
{
    if (face && !m_backing.hasFace(face->backing()))
        m_backing.add(face->backing());
    return *this;
}

bool FontFaceSet::remove(FontFace* face)
{
    if (!face)
        return false;

    bool result = m_backing.hasFace(face->backing());
    if (result)
        m_backing.remove(face->backing());
    return result;
}

void FontFaceSet::clear()
{
    while (m_backing.faceCount())
        m_backing.remove(m_backing[0]);
}

void FontFaceSet::load(const String& font, const String& text, DeferredWrapper&& promise, ExceptionCode& ec)
{
    auto matchingFaces = m_backing.matchingFaces(font, text, ec);
    if (ec)
        return;

    if (matchingFaces.isEmpty()) {
        promise.resolve(Vector<RefPtr<FontFace>>());
        return;
    }

    for (auto& face : matchingFaces)
        face.get().load();

    auto pendingPromise = PendingPromise::create(WTFMove(promise));
    bool waiting = false;

    for (auto& face : matchingFaces) {
        if (face.get().status() == CSSFontFace::Status::Failure) {
            pendingPromise->promise.reject(DOMCoreException::create(ExceptionCodeDescription(NETWORK_ERR)));
            return;
        }
    }

    for (auto& face : matchingFaces) {
        pendingPromise->faces.append(face.get().wrapper());
        if (face.get().status() == CSSFontFace::Status::Success)
            continue;
        waiting = true;
        auto& vector = m_pendingPromises.add(RefPtr<FontFace>(face.get().wrapper()), Vector<Ref<PendingPromise>>()).iterator->value;
        vector.append(pendingPromise.copyRef());
    }

    if (!waiting)
        pendingPromise->promise.resolve(pendingPromise->faces);
}

bool FontFaceSet::check(const String& family, const String& text, ExceptionCode& ec)
{
    return m_backing.check(family, text, ec);
}

auto FontFaceSet::promise(JSC::ExecState& execState) -> Promise&
{
    if (!m_promise) {
        m_promise = createPromise(execState);
        if (m_backing.status() == CSSFontFaceSet::Status::Loaded)
            fulfillPromise();
    }
    return m_promise.value();
}
    
String FontFaceSet::status() const
{
    switch (m_backing.status()) {
    case CSSFontFaceSet::Status::Loading:
        return String("loading", String::ConstructFromLiteral);
    case CSSFontFaceSet::Status::Loaded:
        return String("loaded", String::ConstructFromLiteral);
    }
    ASSERT_NOT_REACHED();
    return String("loaded", String::ConstructFromLiteral);
}

bool FontFaceSet::canSuspendForDocumentSuspension() const
{
    return m_backing.status() == CSSFontFaceSet::Status::Loaded;
}

void FontFaceSet::startedLoading()
{
    // FIXME: Fire a "loading" event asynchronously.
}

void FontFaceSet::completedLoading()
{
    if (m_promise)
        fulfillPromise();
    m_promise = Nullopt;
    // FIXME: Fire a "loadingdone" and possibly a "loadingerror" event asynchronously.
}

void FontFaceSet::fulfillPromise()
{
    // Normally, DeferredWrapper::callFunction resets the reference to the promise.
    // However, API semantics require our promise to live for the entire lifetime of the FontFace.
    // Let's make sure it stays alive.

    Promise guard(m_promise.value());
    m_promise.value().resolve(*this);
    m_promise = guard;
}

void FontFaceSet::faceFinished(CSSFontFace& face, CSSFontFace::Status newStatus)
{
    auto iterator = m_pendingPromises.find(face.wrapper());
    if (iterator == m_pendingPromises.end())
        return;

    for (auto& pendingPromise : iterator->value) {
        if (newStatus == CSSFontFace::Status::Success) {
            if (pendingPromise->hasOneRef())
                pendingPromise->promise.resolve(pendingPromise->faces);
        } else {
            ASSERT(newStatus == CSSFontFace::Status::Failure);
            // The first resolution wins, so we can just reject early now.
            pendingPromise->promise.reject(DOMCoreException::create(ExceptionCodeDescription(NETWORK_ERR)));
        }
    }

    m_pendingPromises.remove(iterator);
}

}
