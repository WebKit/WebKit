/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "AuxiliaryBarrier.h"
#include "JSObject.h"
#include <wtf/TaggedArrayStoragePtr.h>

namespace JSC {

class LLIntOffsetsExtractor;

// This class serves two purposes:
//
// 1) It provides those parts of JSGenericTypedArrayView that don't depend
//    on template parameters.
//
// 2) It represents the DOM/WebCore-visible "JSArrayBufferView" type, which
//    C++ code uses when it wants to pass around a view of an array buffer
//    without concern for the actual type of the view.
//
// These two roles are quite different. (1) is just a matter of optimizing
// compile and link times by having as much code and data as possible not
// be subject to template specialization. (2) is *almost* a matter of
// semantics; indeed at the very least it is a matter of obeying a contract
// that we have with WebCore right now.
//
// One convenient thing that saves us from too much crazy is that
// ArrayBufferView is not instantiable.

// Typed array views have different modes depending on how big they are and
// whether the user has done anything that requires a separate backing
// buffer or the DOM-specified detaching capabilities.
enum TypedArrayMode : uint32_t {
    // Legend:
    // B: JSArrayBufferView::m_butterfly pointer
    // V: JSArrayBufferView::m_vector pointer
    // M: JSArrayBufferView::m_mode

    // Small and fast typed array. B is unused, V points to a vector
    // allocated in the primitive Gigacage, and M = FastTypedArray. V's
    // liveness is determined entirely by the view's liveness.
    FastTypedArray,

    // A large typed array that still attempts not to waste too much
    // memory. B is unused, V points to a vector allocated using
    // Gigacage::tryMalloc(), and M = OversizeTypedArray. V's liveness is
    // determined entirely by the view's liveness, and the view will add a
    // finalizer to delete V.
    OversizeTypedArray,

    // A typed array that was used in some crazy way. B's IndexingHeader
    // is hijacked to contain a reference to the native array buffer. The
    // native typed array view points back to the JS view. V points to a
    // vector allocated using who-knows-what, and M = WastefulTypedArray.
    // The view does not own the vector.
    WastefulTypedArray,

    // A data view. B is unused, V points to a vector allocated using who-
    // knows-what, and M = DataViewMode. The view does not own the vector.
    // There is an extra field (in JSDataView) that points to the
    // ArrayBuffer.
    DataViewMode
};

inline bool hasArrayBuffer(TypedArrayMode mode)
{
    return mode >= WastefulTypedArray;
}

// When WebCore uses a JSArrayBufferView, it expects to be able to get the native
// ArrayBuffer and little else. This requires slowing down and wasting memory,
// and then accessing things via the Butterfly. When JS uses a JSArrayBufferView
// it is always via the usual methods in the MethodTable, so this class's
// implementation of those has no need to even exist - we could at any time sink
// code into JSGenericTypedArrayView if it was convenient.

class JSArrayBufferView : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;

    template<typename, SubspaceAccess>
    static void subspaceFor(VM&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    static constexpr unsigned fastSizeLimit = 1000;
    using VectorPtr = CagedBarrierPtr<Gigacage::Primitive, void, tagCagedPtr>;

    static void* nullVectorPtr()
    {
        VectorPtr null { };
        return null.rawBits();
    }
    
    static size_t sizeOf(uint32_t length, uint32_t elementSize)
    {
        return (static_cast<size_t>(length) * elementSize + sizeof(EncodedJSValue) - 1)
            & ~(sizeof(EncodedJSValue) - 1);
    }

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, !inlineCapacity);
        return sizeof(JSArrayBufferView);
    }
        
protected:
    class ConstructionContext {
        WTF_MAKE_NONCOPYABLE(ConstructionContext);
        
    public:
        enum InitializationMode { ZeroFill, DontInitialize };
        
        JS_EXPORT_PRIVATE ConstructionContext(VM&, Structure*, uint32_t length, uint32_t elementSize, InitializationMode = ZeroFill);
        
        // This is only for constructing fast typed arrays. It's used by the JIT's slow path.
        ConstructionContext(Structure*, uint32_t length, void* vector);
        
        JS_EXPORT_PRIVATE ConstructionContext(
            VM&, Structure*, RefPtr<ArrayBuffer>&&,
            unsigned byteOffset, unsigned length);
        
        enum DataViewTag { DataView };
        ConstructionContext(
            Structure*, RefPtr<ArrayBuffer>&&,
            unsigned byteOffset, unsigned length, DataViewTag);
        
        bool operator!() const { return !m_structure; }
        
        Structure* structure() const { return m_structure; }
        void* vector() const { return m_vector.getMayBeNull(m_length); }
        uint32_t length() const { return m_length; }
        TypedArrayMode mode() const { return m_mode; }
        Butterfly* butterfly() const { return m_butterfly; }
        
    private:
        Structure* m_structure;
        using VectorType = CagedPtr<Gigacage::Primitive, void, tagCagedPtr>;
        VectorType m_vector;
        uint32_t m_length;
        TypedArrayMode m_mode;
        Butterfly* m_butterfly;
    };
    
    JS_EXPORT_PRIVATE JSArrayBufferView(VM&, ConstructionContext&);
    JS_EXPORT_PRIVATE void finishCreation(VM&);

    DECLARE_VISIT_CHILDREN;
    
public:
    TypedArrayMode mode() const { return m_mode; }
    bool hasArrayBuffer() const { return JSC::hasArrayBuffer(mode()); }
    
    bool isShared();
    JS_EXPORT_PRIVATE ArrayBuffer* unsharedBuffer();
    inline ArrayBuffer* possiblySharedBuffer();
    JSArrayBuffer* unsharedJSBuffer(JSGlobalObject* globalObject);
    JSArrayBuffer* possiblySharedJSBuffer(JSGlobalObject* globalObject);
    RefPtr<ArrayBufferView> unsharedImpl();
    JS_EXPORT_PRIVATE RefPtr<ArrayBufferView> possiblySharedImpl();
    bool isDetached() { return hasArrayBuffer() && !hasVector(); }
    void detach();

    bool hasVector() const { return !!m_vector; }
    void* vector() const { return m_vector.getMayBeNull(length()); }
    void* vectorWithoutPACValidation() const { return m_vector.getUnsafe(); }
    
    inline unsigned byteOffset();
    inline std::optional<unsigned> byteOffsetConcurrently();

    unsigned length() const { return m_length; }
    unsigned byteLength() const;

    DECLARE_EXPORT_INFO;
    
    static ptrdiff_t offsetOfVector() { return OBJECT_OFFSETOF(JSArrayBufferView, m_vector); }
    static ptrdiff_t offsetOfLength() { return OBJECT_OFFSETOF(JSArrayBufferView, m_length); }
    static ptrdiff_t offsetOfMode() { return OBJECT_OFFSETOF(JSArrayBufferView, m_mode); }
    
    static RefPtr<ArrayBufferView> toWrapped(VM&, JSValue);
    static RefPtr<ArrayBufferView> toWrappedAllowShared(VM&, JSValue);

private:
    enum Requester { Mutator, ConcurrentThread };
    template<Requester, typename ResultType> ResultType byteOffsetImpl();
    template<Requester> ArrayBuffer* possiblySharedBufferImpl();

    JS_EXPORT_PRIVATE ArrayBuffer* slowDownAndWasteMemory();
    static void finalize(JSCell*);

protected:
    friend class LLIntOffsetsExtractor;

    ArrayBuffer* existingBufferInButterfly();

    VectorPtr m_vector;
    uint32_t m_length;
    TypedArrayMode m_mode;
};

JSArrayBufferView* validateTypedArray(JSGlobalObject*, JSValue);

} // namespace JSC

namespace WTF {

JS_EXPORT_PRIVATE void printInternal(PrintStream&, JSC::TypedArrayMode);

} // namespace WTF
