/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CrossThreadCopier_h
#define CrossThreadCopier_h

#include <memory>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>
#include <wtf/TypeTraits.h>

namespace WebCore {

    class ResourceError;
    class ResourceRequest;
    class ResourceResponse;
    class String;
    struct CrossThreadResourceResponseData;
    struct CrossThreadResourceRequestData;
    struct ThreadableLoaderOptions;

    template<typename T> struct CrossThreadCopierPassThrough {
        typedef T Type;
        static Type copy(const T& parameter)
        {
            return parameter;
        }
    };

    template<bool isConvertibleToInteger, typename T> struct CrossThreadCopierBase;

    // Integers get passed through without any changes.
    template<typename T> struct CrossThreadCopierBase<true, T> : public CrossThreadCopierPassThrough<T> {
    };

    // Pointers get passed through without any significant changes.
    template<typename T> struct CrossThreadCopierBase<false, T*> : public CrossThreadCopierPassThrough<T*> {
    };

    template<> struct CrossThreadCopierBase<false, ThreadableLoaderOptions> : public CrossThreadCopierPassThrough<ThreadableLoaderOptions> {
    };

    // Custom copy methods.
    template<typename T> struct CrossThreadCopierBase<false, RefPtr<ThreadSafeShared<T> > > {
        typedef PassRefPtr<T> Type;
        static Type copy(const RefPtr<ThreadSafeShared<T> >& refPtr)
        {
            return PassRefPtr<T>(static_cast<T*>(refPtr.get()));
        }
    };

    template<typename T> struct CrossThreadCopierBase<false, PassOwnPtr<T> > {
        typedef PassOwnPtr<T> Type;
        static Type copy(const PassOwnPtr<T>& ownPtr)
        {
            return PassOwnPtr<T>(static_cast<T*>(ownPtr.release()));
        }
    };

    template<typename T> struct CrossThreadCopierBase<false, std::auto_ptr<T> > {
        typedef std::auto_ptr<T> Type;
        static Type copy(const std::auto_ptr<T>& autoPtr)
        {
            return std::auto_ptr<T>(*const_cast<std::auto_ptr<T>*>(&autoPtr));
        }
    };

    template<> struct CrossThreadCopierBase<false, String> {
        typedef String Type;
        static Type copy(const String&);
    };

    template<> struct CrossThreadCopierBase<false, ResourceError> {
        typedef ResourceError Type;
        static Type copy(const ResourceError&);
    };

    template<> struct CrossThreadCopierBase<false, ResourceRequest> {
        typedef std::auto_ptr<CrossThreadResourceRequestData> Type;
        static Type copy(const ResourceRequest&);
    };

    template<> struct CrossThreadCopierBase<false, ResourceResponse> {
        typedef std::auto_ptr<CrossThreadResourceResponseData> Type;
        static Type copy(const ResourceResponse&);
    };

    template<typename T> struct CrossThreadCopier : public CrossThreadCopierBase<WTF::IsConvertibleToInteger<T>::value, T> {
    };

} // namespace WebCore

#endif // CrossThreadCopier_h
