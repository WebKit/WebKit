//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexArray11.h: Defines the rx::VertexArray11 class which implements rx::VertexArrayImpl.

#ifndef LIBANGLE_RENDERER_D3D_D3D11_VERTEXARRAY11_H_
#define LIBANGLE_RENDERER_D3D_D3D11_VERTEXARRAY11_H_

#include "libANGLE/Framebuffer.h"
#include "libANGLE/renderer/VertexArrayImpl.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"
#include "libANGLE/signal_utils.h"

namespace rx
{
class Renderer11;

class VertexArray11 : public VertexArrayImpl, public OnBufferDataDirtyReceiver
{
  public:
    VertexArray11(const gl::VertexArrayState &data);
    ~VertexArray11() override;
    void destroy(const gl::Context *context) override;

    void syncState(const gl::Context *context,
                   const gl::VertexArray::DirtyBits &dirtyBits) override;
    // This will flush any pending attrib updates and then check the dynamic attribs mask.
    bool hasActiveDynamicAttrib(const gl::Context *context);
    gl::Error updateDirtyAndDynamicAttribs(const gl::Context *context,
                                           VertexDataManager *vertexDataManager,
                                           const DrawCallVertexParams &vertexParams);
    void clearDirtyAndPromoteDynamicAttribs(const gl::Context *context,
                                            const DrawCallVertexParams &vertexParams);

    const std::vector<TranslatedAttribute> &getTranslatedAttribs() const;

    // SignalReceiver implementation
    void signal(size_t channelID, const gl::Context *context) override;

    Serial getCurrentStateSerial() const { return mCurrentStateSerial; }

    // In case of a multi-view program change, we have to update all attributes so that the divisor
    // is adjusted.
    void markAllAttributeDivisorsForAdjustment(int numViews);

    bool flushAttribUpdates(const gl::Context *context);

    // Returns true if the element array buffer needs to be translated.
    bool updateElementArrayStorage(const gl::Context *context,
                                   GLenum elementType,
                                   GLenum destElementType,
                                   const void *indices);

    TranslatedIndexData *getCachedIndexInfo();
    void setCachedIndexInfoValid();
    bool isCachedIndexInfoValid() const;

  private:
    void updateVertexAttribStorage(const gl::Context *context, size_t attribIndex);

    std::vector<VertexStorageType> mAttributeStorageTypes;
    std::vector<TranslatedAttribute> mTranslatedAttribs;

    // The mask of attributes marked as dynamic.
    gl::AttributesMask mDynamicAttribsMask;

    // A mask of attributes that need to be re-evaluated.
    gl::AttributesMask mAttribsToUpdate;

    // A set of attributes we know are dirty, and need to be re-translated.
    gl::AttributesMask mAttribsToTranslate;

    // We need to keep a safe pointer to the Buffer so we can attach the correct dirty callbacks.
    std::vector<gl::BindingPointer<gl::Buffer>> mCurrentArrayBuffers;
    gl::BindingPointer<gl::Buffer> mCurrentElementArrayBuffer;

    std::vector<OnBufferDataDirtyBinding> mOnArrayBufferDataDirty;
    OnBufferDataDirtyBinding mOnElementArrayBufferDataDirty;

    Serial mCurrentStateSerial;

    // The numViews value used to adjust the divisor.
    int mAppliedNumViewsToDivisor;

    // If the index buffer needs re-streaming.
    GLenum mLastElementType;
    unsigned int mLastDrawElementsOffset;
    IndexStorageType mCurrentElementArrayStorage;
    TranslatedIndexData mCachedIndexInfo;
    bool mCachedIndexInfoValid;
};

}  // namespace rx

#endif // LIBANGLE_RENDERER_D3D_D3D11_VERTEXARRAY11_H_
