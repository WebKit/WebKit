/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCDrawQuad.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCGeometryTestUtils.h"
#include "CCIOSurfaceDrawQuad.h"
#include "CCRenderPassDrawQuad.h"
#include "CCSolidColorDrawQuad.h"
#include "CCStreamVideoDrawQuad.h"
#include "CCTextureDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "CCYUVVideoDrawQuad.h"
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

using WebKit::WebTransformationMatrix;

using namespace WebCore;

namespace {

TEST(CCDrawQuadTest, copySharedQuadState)
{
    WebTransformationMatrix quadTransform(1, 0.5, 0, 1, 0.5, 0);
    IntRect visibleContentRect(10, 12, 14, 16);
    IntRect clippedRectInTarget(19, 21, 23, 25);
    float opacity = 0.25;
    bool opaque = true;
    int id = 3;

    OwnPtr<CCSharedQuadState> state(CCSharedQuadState::create(quadTransform, visibleContentRect, clippedRectInTarget, opacity, opaque));
    state->id = id;

    OwnPtr<CCSharedQuadState> copy(state->copy());
    EXPECT_EQ(id, copy->id);
    EXPECT_EQ(quadTransform, copy->quadTransform);
    EXPECT_RECT_EQ(visibleContentRect, copy->visibleContentRect);
    EXPECT_RECT_EQ(clippedRectInTarget, copy->clippedRectInTarget);
    EXPECT_EQ(opacity, copy->opacity);
    EXPECT_EQ(opaque, copy->opaque);
}

PassOwnPtr<CCSharedQuadState> createSharedQuadState()
{
    WebTransformationMatrix quadTransform(1, 0.5, 0, 1, 0.5, 0);
    IntRect visibleContentRect(10, 12, 14, 16);
    IntRect clippedRectInTarget(19, 21, 23, 25);
    float opacity = 1;
    bool opaque = false;
    int id = 3;

    OwnPtr<CCSharedQuadState> state(CCSharedQuadState::create(quadTransform, visibleContentRect, clippedRectInTarget, opacity, opaque));
    state->id = id;
    return state.release();
}

void compareDrawQuad(CCDrawQuad* quad, CCDrawQuad* copy, CCSharedQuadState* copySharedState)
{
    EXPECT_EQ(quad->size(), copy->size());
    EXPECT_EQ(quad->material(), copy->material());
    EXPECT_EQ(quad->isDebugQuad(), copy->isDebugQuad());
    EXPECT_RECT_EQ(quad->quadRect(), copy->quadRect());
    EXPECT_RECT_EQ(quad->quadVisibleRect(), copy->quadVisibleRect());
    EXPECT_EQ(quad->opaqueRect(), copy->opaqueRect());
    EXPECT_EQ(quad->needsBlending(), copy->needsBlending());

    EXPECT_EQ(copySharedState, copy->sharedQuadState());
    EXPECT_EQ(copySharedState->id, copy->sharedQuadStateId());

    EXPECT_EQ(quad->sharedQuadStateId(), quad->sharedQuadState()->id);
    EXPECT_EQ(copy->sharedQuadStateId(), copy->sharedQuadState()->id);
}

#define CREATE_SHARED_STATE() \
    OwnPtr<CCSharedQuadState> sharedState(createSharedQuadState()); \
    OwnPtr<CCSharedQuadState> copySharedState(sharedState->copy()); \
    copySharedState->id = 5;

#define QUAD_DATA \
    IntRect quadRect(30, 40, 50, 60); \
    IntRect quadVisibleRect(40, 50, 30, 20); \

#define SETUP_AND_COPY_QUAD(Type, quad) \
    quad->setQuadVisibleRect(quadVisibleRect); \
    OwnPtr<CCDrawQuad> copy(quad->copy(copySharedState.get())); \
    compareDrawQuad(quad.get(), copy.get(), copySharedState.get()); \
    const Type* copyQuad = Type::materialCast(copy.get());

#define SETUP_AND_COPY_QUAD_1(Type, quad, a) \
    quad->setQuadVisibleRect(quadVisibleRect); \
    OwnPtr<CCDrawQuad> copy(quad->copy(copySharedState.get(), a));    \
    compareDrawQuad(quad.get(), copy.get(), copySharedState.get()); \
    const Type* copyQuad = Type::materialCast(copy.get());

#define CREATE_QUAD_0(Type) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect)); \
    SETUP_AND_COPY_QUAD(Type, quad); \
    UNUSED_PARAM(copyQuad);

#define CREATE_QUAD_1(Type, a) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_2(Type, a, b) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_3(Type, a, b, c) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_4(Type, a, b, c, d) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_5(Type, a, b, c, d, e) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d, e)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_6(Type, a, b, c, d, e, f) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d, e, f)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_7(Type, a, b, c, d, e, f, g) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d, e, f, g)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_8(Type, a, b, c, d, e, f, g, h) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d, e, f, g, h)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_8_1(Type, a, b, c, d, e, f, g, h, copyA) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d, e, f, g, h)); \
    SETUP_AND_COPY_QUAD_1(Type, quad, copyA);

#define CREATE_QUAD_9(Type, a, b, c, d, e, f, g, h, i) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d, e, f, g, h, i)); \
    SETUP_AND_COPY_QUAD(Type, quad);

#define CREATE_QUAD_10(Type, a, b, c, d, e, f, g, h, i, j) \
    QUAD_DATA \
    OwnPtr<Type> quad(Type::create(sharedState.get(), quadRect, a, b, c, d, e, f, g, h, i, j)); \
    SETUP_AND_COPY_QUAD(Type, quad);

TEST(CCDrawQuadTest, copyCheckerboardDrawQuad)
{
    CREATE_SHARED_STATE();
    CREATE_QUAD_0(CCCheckerboardDrawQuad);
}

TEST(CCDrawQuadTest, copyDebugBorderDrawQuad)
{
    SkColor color = 0xfabb0011;
    int width = 99;
    CREATE_SHARED_STATE();
    CREATE_QUAD_2(CCDebugBorderDrawQuad, color, width);
    EXPECT_EQ(color, copyQuad->color());
    EXPECT_EQ(width, copyQuad->width());
}

TEST(CCDrawQuadTest, copyIOSurfaceDrawQuad)
{
    IntSize size(58, 95);
    unsigned textureId = 72;
    CCIOSurfaceDrawQuad::Orientation orientation = CCIOSurfaceDrawQuad::Unflipped;

    CREATE_SHARED_STATE();
    CREATE_QUAD_3(CCIOSurfaceDrawQuad, size, textureId, orientation);
    EXPECT_EQ(size, copyQuad->ioSurfaceSize());
    EXPECT_EQ(textureId, copyQuad->ioSurfaceTextureId());
    EXPECT_EQ(orientation, copyQuad->orientation());
}

TEST(CCDrawQuadTest, copyRenderPassDrawQuad)
{
    CCRenderPass::Id renderPassId(22, 64);
    bool isReplica = true;
    CCResourceProvider::ResourceId maskResourceId = 78;
    IntRect contentsChangedSinceLastFrame(42, 11, 74, 24);
    float maskTexCoordScaleX = 33;
    float maskTexCoordScaleY = 19;
    float maskTexCoordOffsetX = -45;
    float maskTexCoordOffsetY = -21;

    CCRenderPass::Id copiedRenderPassId(235, 11);

    CREATE_SHARED_STATE();
    CREATE_QUAD_8_1(CCRenderPassDrawQuad, renderPassId, isReplica, maskResourceId, contentsChangedSinceLastFrame, maskTexCoordScaleX, maskTexCoordScaleY, maskTexCoordOffsetX, maskTexCoordOffsetY, copiedRenderPassId);
    EXPECT_EQ(copiedRenderPassId, copyQuad->renderPassId());
    EXPECT_EQ(isReplica, copyQuad->isReplica());
    EXPECT_EQ(maskResourceId, copyQuad->maskResourceId());
    EXPECT_RECT_EQ(contentsChangedSinceLastFrame, copyQuad->contentsChangedSinceLastFrame());
    EXPECT_EQ(maskTexCoordScaleX, copyQuad->maskTexCoordScaleX());
    EXPECT_EQ(maskTexCoordScaleY, copyQuad->maskTexCoordScaleY());
    EXPECT_EQ(maskTexCoordOffsetX, copyQuad->maskTexCoordOffsetX());
    EXPECT_EQ(maskTexCoordOffsetY, copyQuad->maskTexCoordOffsetY());
}

TEST(CCDrawQuadTest, copySolidColorDrawQuad)
{
    SkColor color = 0x49494949;

    CREATE_SHARED_STATE();
    CREATE_QUAD_1(CCSolidColorDrawQuad, color);
    EXPECT_EQ(color, copyQuad->color());
}

TEST(CCDrawQuadTest, copyStreamVideoDrawQuad)
{
    unsigned textureId = 64;
    WebTransformationMatrix matrix(0.5, 1, 0.25, 0.75, 0, 1);

    CREATE_SHARED_STATE();
    CREATE_QUAD_2(CCStreamVideoDrawQuad, textureId, matrix);
    EXPECT_EQ(textureId, copyQuad->textureId());
    EXPECT_EQ(matrix, copyQuad->matrix());
}

TEST(CCDrawQuadTest, copyTextureDrawQuad)
{
    unsigned resourceId = 82;
    bool premultipliedAlpha = true;
    FloatRect uvRect(0.5, 224, -51, 36);
    bool flipped = true;

    CREATE_SHARED_STATE();
    CREATE_QUAD_4(CCTextureDrawQuad, resourceId, premultipliedAlpha, uvRect, flipped);
    EXPECT_EQ(resourceId, copyQuad->resourceId());
    EXPECT_EQ(premultipliedAlpha, copyQuad->premultipliedAlpha());
    EXPECT_EQ(uvRect, copyQuad->uvRect());
    EXPECT_EQ(flipped, copyQuad->flipped());
}

TEST(CCDrawQuadTest, copyTileDrawQuad)
{
    IntRect opaqueRect(33, 44, 22, 33);
    unsigned resourceId = 104;
    IntPoint textureOffset(-31, 47);
    IntSize textureSize(85, 32);
    GC3Dint textureFilter = 82;
    bool swizzleContents = true;
    bool leftEdgeAA = true;
    bool topEdgeAA = true;
    bool rightEdgeAA = false;
    bool bottomEdgeAA = true;

    CREATE_SHARED_STATE();
    CREATE_QUAD_10(CCTileDrawQuad, opaqueRect, resourceId, textureOffset, textureSize, textureFilter, swizzleContents, leftEdgeAA, topEdgeAA, rightEdgeAA, bottomEdgeAA);
    EXPECT_RECT_EQ(opaqueRect, copyQuad->opaqueRect());
    EXPECT_EQ(resourceId, copyQuad->resourceId());
    EXPECT_EQ(textureOffset, copyQuad->textureOffset());
    EXPECT_EQ(textureSize, copyQuad->textureSize());
    EXPECT_EQ(textureFilter, copyQuad->textureFilter());
    EXPECT_EQ(swizzleContents, copyQuad->swizzleContents());
    EXPECT_EQ(leftEdgeAA, copyQuad->leftEdgeAA());
    EXPECT_EQ(topEdgeAA, copyQuad->topEdgeAA());
    EXPECT_EQ(rightEdgeAA, copyQuad->rightEdgeAA());
    EXPECT_EQ(bottomEdgeAA, copyQuad->bottomEdgeAA());
}

TEST(CCDrawQuadTest, copyYUVVideoDrawQuad)
{
    CCVideoLayerImpl::FramePlane yPlane;
    yPlane.resourceId = 45;
    yPlane.size = IntSize(34, 23);
    yPlane.format = 8;
    yPlane.visibleSize = IntSize(623, 235);
    CCVideoLayerImpl::FramePlane uPlane;
    uPlane.resourceId = 532;
    uPlane.size = IntSize(134, 16);
    uPlane.format = 2;
    uPlane.visibleSize = IntSize(126, 27);
    CCVideoLayerImpl::FramePlane vPlane;
    vPlane.resourceId = 4;
    vPlane.size = IntSize(456, 486);
    vPlane.format = 46;
    vPlane.visibleSize = IntSize(19, 45);

    CREATE_SHARED_STATE();
    CREATE_QUAD_3(CCYUVVideoDrawQuad, yPlane, uPlane, vPlane);
    EXPECT_EQ(yPlane.resourceId, copyQuad->yPlane().resourceId);
    EXPECT_EQ(yPlane.size, copyQuad->yPlane().size);
    EXPECT_EQ(yPlane.format, copyQuad->yPlane().format);
    EXPECT_EQ(yPlane.visibleSize, copyQuad->yPlane().visibleSize);
    EXPECT_EQ(uPlane.resourceId, copyQuad->uPlane().resourceId);
    EXPECT_EQ(uPlane.size, copyQuad->uPlane().size);
    EXPECT_EQ(uPlane.format, copyQuad->uPlane().format);
    EXPECT_EQ(uPlane.visibleSize, copyQuad->uPlane().visibleSize);
    EXPECT_EQ(vPlane.resourceId, copyQuad->vPlane().resourceId);
    EXPECT_EQ(vPlane.size, copyQuad->vPlane().size);
    EXPECT_EQ(vPlane.format, copyQuad->vPlane().format);
    EXPECT_EQ(vPlane.visibleSize, copyQuad->vPlane().visibleSize);
}

} // namespace
