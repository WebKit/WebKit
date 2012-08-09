/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "cc/CCRenderSurface.h"

#include "cc/CCLayerImpl.h"
#include "cc/CCSharedQuadState.h"
#include "cc/CCSingleThreadProxy.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

using namespace WebCore;
using WebKit::WebTransformationMatrix;

namespace {

#define EXECUTE_AND_VERIFY_SURFACE_CHANGED(codeToTest)                  \
    renderSurface->resetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_TRUE(renderSurface->surfacePropertyChanged())

#define EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(codeToTest)           \
    renderSurface->resetPropertyChangedFlag();                          \
    codeToTest;                                                         \
    EXPECT_FALSE(renderSurface->surfacePropertyChanged())

TEST(CCRenderSurfaceTest, verifySurfaceChangesAreTrackedProperly)
{
    //
    // This test checks that surfacePropertyChanged() has the correct behavior.
    //

    // This will fake that we are on the correct thread for testing purposes.
    DebugScopedSetImplThread setImplThread;

    OwnPtr<CCLayerImpl> owningLayer = CCLayerImpl::create(1);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    CCRenderSurface* renderSurface = owningLayer->renderSurface();
    IntRect testRect = IntRect(IntPoint(3, 4), IntSize(5, 6));
    owningLayer->resetAllChangeTrackingForSubtree();

    // Currently, the contentRect, clipRect, and owningLayer->layerPropertyChanged() are
    // the only sources of change.
    EXECUTE_AND_VERIFY_SURFACE_CHANGED(renderSurface->setClipRect(testRect));
    EXECUTE_AND_VERIFY_SURFACE_CHANGED(renderSurface->setContentRect(testRect));

    owningLayer->setOpacity(0.5f);
    EXPECT_TRUE(renderSurface->surfacePropertyChanged());
    owningLayer->resetAllChangeTrackingForSubtree();

    // Setting the surface properties to the same values again should not be considered "change".
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setClipRect(testRect));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setContentRect(testRect));

    OwnPtr<CCLayerImpl> dummyMask = CCLayerImpl::create(1);
    WebTransformationMatrix dummyMatrix;
    dummyMatrix.translate(1.0, 2.0);

    // The rest of the surface properties are either internal and should not cause change,
    // or they are already accounted for by the owninglayer->layerPropertyChanged().
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawOpacity(0.5));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setReplicaDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->clearLayerList());
}

TEST(CCRenderSurfaceTest, sanityCheckSurfaceCreatesCorrectSharedQuadState)
{
    // This will fake that we are on the correct thread for testing purposes.
    DebugScopedSetImplThread setImplThread;

    OwnPtr<CCLayerImpl> rootLayer = CCLayerImpl::create(1);

    OwnPtr<CCLayerImpl> owningLayer = CCLayerImpl::create(2);
    owningLayer->createRenderSurface();
    ASSERT_TRUE(owningLayer->renderSurface());
    owningLayer->setRenderTarget(owningLayer.get());
    CCRenderSurface* renderSurface = owningLayer->renderSurface();

    rootLayer->addChild(owningLayer.release());

    IntRect contentRect = IntRect(IntPoint::zero(), IntSize(50, 50));
    IntRect clipRect = IntRect(IntPoint(5, 5), IntSize(40, 40));
    WebTransformationMatrix origin;

    origin.translate(30, 40);

    renderSurface->setDrawTransform(origin);
    renderSurface->setContentRect(contentRect);
    renderSurface->setClipRect(clipRect);
    renderSurface->setDrawOpacity(1);

    OwnPtr<CCSharedQuadState> sharedQuadState = renderSurface->createSharedQuadState(0);

    EXPECT_EQ(30, sharedQuadState->quadTransform.m41());
    EXPECT_EQ(40, sharedQuadState->quadTransform.m42());
    EXPECT_EQ(contentRect, IntRect(sharedQuadState->visibleContentRect));
    EXPECT_EQ(1, sharedQuadState->opacity);
    EXPECT_FALSE(sharedQuadState->opaque);
}

} // namespace
