/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/Forward.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Threading.h>

namespace WebCore {

    class IntRect;
    class IntSize;
    class URL;
    class ResourceError;
    class ResourceRequest;
    class ResourceResponse;
    struct CrossThreadResourceResponseData;
    struct CrossThreadResourceRequestData;
    struct ThreadableLoaderOptions;

    struct CrossThreadCopierBaseHelper {
        template<typename T> struct RemovePointer {
            typedef T Type;
        };
        template<typename T> struct RemovePointer<T*> {
            typedef T Type;
        };

        template<typename T> struct RemovePointer<RefPtr<T>> {
            typedef T Type;
        };

        template<typename T> struct RemovePointer<PassRefPtr<T>> {
            typedef T Type;
        };

        template<typename T> struct IsEnumOrConvertibleToInteger {
            static const bool value = std::is_integral<T>::value || std::is_enum<T>::value || std::is_convertible<T, long double>::value;
        };

        template<typename T> struct IsThreadSafeRefCountedPointer {
            static const bool value = std::is_convertible<typename RemovePointer<T>::Type*, ThreadSafeRefCounted<typename RemovePointer<T>::Type>*>::value;
        };
    };

    template<typename T> struct CrossThreadCopierPassThrough {
        typedef T Type;
        static Type copy(const T& parameter)
        {
            return parameter;
        }
    };

    template<bool isEnumOrConvertibleToInteger, bool isThreadSafeRefCounted, typename T> struct CrossThreadCopierBase;

    // Integers get passed through without any changes.
    template<typename T> struct CrossThreadCopierBase<true, false, T> : public CrossThreadCopierPassThrough<T> {
    };

    // To allow a type to be passed across threads using its copy constructor, add a forward declaration of the type and
    // a CopyThreadCopierBase<false, false, TypeName> : public CrossThreadCopierPassThrough<TypeName> { }; to this file.
    template<> struct CrossThreadCopierBase<false, false, ThreadableLoaderOptions> : public CrossThreadCopierPassThrough<ThreadableLoaderOptions> {
    };

    template<> struct CrossThreadCopierBase<false, false, IntRect> : public CrossThreadCopierPassThrough<IntRect> {
    };

    template<> struct CrossThreadCopierBase<false, false, IntSize> : public CrossThreadCopierPassThrough<IntSize> {
    };

    // Custom copy methods.
    template<typename T> struct CrossThreadCopierBase<false, true, T> {
        typedef typename CrossThreadCopierBaseHelper::RemovePointer<T>::Type RefCountedType;
        static_assert(std::is_convertible<RefCountedType*, ThreadSafeRefCounted<RefCountedType>*>::value, "T is not convertible to ThreadSafeRefCounted!");

        typedef PassRefPtr<RefCountedType> Type;
        static Type copy(const T& refPtr)
        {
            return refPtr;
        }
    };

    template<typename T> struct CrossThreadCopierBase<false, false, PassOwnPtr<T>> {
        typedef PassOwnPtr<T> Type;
        static Type copy(Type ownPtr)
        {
            return ownPtr;
        }
    };

    template<> struct CrossThreadCopierBase<false, false, URL> {
        typedef URL Type;
        static Type copy(const URL&);
    };

    template<> struct CrossThreadCopierBase<false, false, String> {
        typedef String Type;
        static Type copy(const String&);
    };

    template<> struct CrossThreadCopierBase<false, false, ResourceError> {
        typedef ResourceError Type;
        static Type copy(const ResourceError&);
    };

    template<> struct CrossThreadCopierBase<false, false, ResourceRequest> {
        typedef PassOwnPtr<CrossThreadResourceRequestData> Type;
        static Type copy(const ResourceRequest&);
    };

    template<> struct CrossThreadCopierBase<false, false, ResourceResponse> {
        typedef PassOwnPtr<CrossThreadResourceResponseData> Type;
        static Type copy(const ResourceResponse&);
    };

#if ENABLE(INDEXED_DATABASE)
    namespace IndexedDB {
        enum class TransactionMode;
        enum class CursorDirection;
        enum class CursorType;
    }
    template<> struct CrossThreadCopierBase<false, false, IndexedDB::TransactionMode> {
        static IndexedDB::TransactionMode copy(const IndexedDB::TransactionMode&);
    };
    template<> struct CrossThreadCopierBase<false, false, IndexedDB::CursorDirection> {
        static IndexedDB::CursorDirection copy(const IndexedDB::CursorDirection&);
    };
    template<> struct CrossThreadCopierBase<false, false, IndexedDB::CursorType> {
        static IndexedDB::CursorType copy(const IndexedDB::CursorType&);
    };

    struct IDBDatabaseMetadata;
    template<> struct CrossThreadCopierBase<false, false, IDBDatabaseMetadata> {
        typedef IDBDatabaseMetadata Type;
        static Type copy(const IDBDatabaseMetadata&);
    };

    struct IDBGetResult;
    template<> struct CrossThreadCopierBase<false, false, IDBGetResult> {
        typedef IDBGetResult Type;
        static Type copy(const IDBGetResult&);
    };

    struct IDBIndexMetadata;
    template<> struct CrossThreadCopierBase<false, false, IDBIndexMetadata> {
        typedef IDBIndexMetadata Type;
        static Type copy(const IDBIndexMetadata&);
    };

    struct IDBKeyData;
    template<> struct CrossThreadCopierBase<false, false, IDBKeyData> {
        typedef IDBKeyData Type;
        static Type copy(const IDBKeyData&);
    };

    struct IDBKeyRangeData;
    template<> struct CrossThreadCopierBase<false, false, IDBKeyRangeData> {
        typedef IDBKeyRangeData Type;
        static Type copy(const IDBKeyRangeData&);
    };

    struct IDBObjectStoreMetadata;
    template<> struct CrossThreadCopierBase<false, false, IDBObjectStoreMetadata> {
        typedef IDBObjectStoreMetadata Type;
        static Type copy(const IDBObjectStoreMetadata&);
    };
#endif

    template<typename T>
    struct CrossThreadCopier : public CrossThreadCopierBase<CrossThreadCopierBaseHelper::IsEnumOrConvertibleToInteger<T>::value, CrossThreadCopierBaseHelper::IsThreadSafeRefCountedPointer<T>::value, T> {
    };

    template<typename T> struct AllowCrossThreadAccessWrapper {
    public:
        explicit AllowCrossThreadAccessWrapper(T* value) : m_value(value) { }
        T* value() const { return m_value; }
    private:
        T* m_value;
    };

    template<typename T> struct CrossThreadCopierBase<false, false, AllowCrossThreadAccessWrapper<T>> {
        typedef T* Type;
        static Type copy(const AllowCrossThreadAccessWrapper<T>& wrapper) { return wrapper.value(); }
    };

    template<typename T> AllowCrossThreadAccessWrapper<T> AllowCrossThreadAccess(T* value) 
    {
        return AllowCrossThreadAccessWrapper<T>(value);
    }

    // FIXME: Move to a different header file. AllowAccessLater is for cross-thread access
    // that is not cross-thread (tasks posted to a queue guaranteed to run on the same thread).
    template<typename T> struct AllowAccessLaterWrapper {
    public:
        explicit AllowAccessLaterWrapper(T* value) : m_value(value) { }
        T* value() const { return m_value; }
    private:
        T* m_value;
    };

    template<typename T> struct CrossThreadCopierBase<false, false, AllowAccessLaterWrapper<T>> {
        typedef T* Type;
        static Type copy(const AllowAccessLaterWrapper<T>& wrapper) { return wrapper.value(); }
    };

    template<typename T> AllowAccessLaterWrapper<T> AllowAccessLater(T* value)
    {
        return AllowAccessLaterWrapper<T>(value);
    }


} // namespace WebCore

#endif // CrossThreadCopier_h
