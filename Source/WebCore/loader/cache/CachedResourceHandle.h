/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/CachedResource.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CachedResourceHandleBase {
public:
    WEBCORE_EXPORT ~CachedResourceHandleBase();

    WEBCORE_EXPORT CachedResource* NODELETE get() const;
    
    bool operator!() const { return !m_resource; }
    operator bool() const { return !!m_resource; }

protected:
    WEBCORE_EXPORT CachedResourceHandleBase();
    WEBCORE_EXPORT explicit CachedResourceHandleBase(RefPtr<CachedResource>&&);
    WEBCORE_EXPORT explicit CachedResourceHandleBase(Ref<CachedResource>&&);
    WEBCORE_EXPORT CachedResourceHandleBase(const CachedResourceHandleBase&);

    WEBCORE_EXPORT void setResource(RefPtr<CachedResource>&&);
    
private:
    CachedResourceHandleBase& operator=(const CachedResourceHandleBase&) { return *this; } 
    
    friend class CachedResource;

    RefPtr<CachedResource> m_resource;
};
    
template <class R> class CachedResourceHandle : public CachedResourceHandleBase {
public: 
    CachedResourceHandle() = default;
    CachedResourceHandle(std::nullptr_t) { }
    CachedResourceHandle(Ref<R>&& res) : CachedResourceHandleBase(Ref<CachedResource> { WTF::move(res) }) { }
    template<typename U> requires (std::is_base_of_v<R, U> && !std::is_same_v<R, U>)
    CachedResourceHandle(Ref<U>&& res) : CachedResourceHandleBase(Ref<CachedResource> { WTF::move(res) }) { }
    CachedResourceHandle(R& res) : CachedResourceHandleBase(Ref<CachedResource> { res }) { }
    CachedResourceHandle(RefPtr<R>&& res) : CachedResourceHandleBase(WTF::move(res)) { }
    template<typename U> requires (std::is_base_of_v<R, U> && !std::is_same_v<R, U>)
    CachedResourceHandle(RefPtr<U>&& res) : CachedResourceHandleBase(RefPtr<CachedResource> { WTF::move(res) }) { }
    CachedResourceHandle(const CachedResourceHandle<R>& o) : CachedResourceHandleBase(o) { }
    template<typename U> CachedResourceHandle(const CachedResourceHandle<U>& o) : CachedResourceHandleBase(o.get()) { }

    R* get() const
    {
        if constexpr (std::same_as<R, CachedResource>)
            return CachedResourceHandleBase::get();
        else
            return downcast<R>(CachedResourceHandleBase::get());
    }
    R* operator->() const { return get(); }
    R& operator*() const { ASSERT(get()); return *get(); }
    operator RefPtr<R>() const { return get(); }

    CachedResourceHandle& operator=(std::nullptr_t) { setResource(nullptr); return *this; }
    CachedResourceHandle& operator=(R& res) { setResource(Ref<CachedResource> { res }); return *this; }
    CachedResourceHandle& operator=(Ref<R>&& res) { setResource(Ref<CachedResource> { WTF::move(res) }); return *this; }
    template<typename U> requires (std::is_base_of_v<R, U> && !std::is_same_v<R, U>)
    CachedResourceHandle& operator=(Ref<U>&& res) { setResource(Ref<CachedResource> { WTF::move(res) }); return *this; }
    CachedResourceHandle& operator=(const RefPtr<R>& res) { setResource(RefPtr<CachedResource> { res.get() }); return *this; }
    CachedResourceHandle& operator=(RefPtr<R>&& res) { setResource(WTF::move(res)); return *this; }
    CachedResourceHandle& operator=(const CachedResourceHandle& o) { setResource(RefPtr<CachedResource> { o.get() }); return *this; }
    template<typename U> CachedResourceHandle& operator=(const CachedResourceHandle<U>& o) { setResource(RefPtr<CachedResource> { o.get() }); return *this; }

    bool operator==(const CachedResourceHandle& o) const { return operator==(static_cast<const CachedResourceHandleBase&>(o)); }
    bool operator==(const CachedResourceHandleBase& o) const { return get() == o.get(); }
};

template <class R, class RR> bool operator==(const CachedResourceHandle<R>& h, const RR* res)
{
    return h.get() == res;
}

template <class R, class RR> bool operator==(const CachedResourceHandle<R>& h, const RefPtr<RR>& ptr)
{
    return h.get() == ptr.get();
}

} // namespace WebCore

namespace WTF {

template<typename R> RefPtr(const WebCore::CachedResourceHandle<R>&) -> RefPtr<R, RawPtrTraits<R>, DefaultRefDerefTraits<R>>;
template<typename R> RefPtr(WebCore::CachedResourceHandle<R>&) -> RefPtr<R, RawPtrTraits<R>, DefaultRefDerefTraits<R>>;

template<typename T>
RefPtr<T> protect(const WebCore::CachedResourceHandle<T>& handle)
{
    return handle.get();
}

} // namespace WTF
