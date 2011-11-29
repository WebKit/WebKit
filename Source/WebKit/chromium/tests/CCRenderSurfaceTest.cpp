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
#include "cc/CCSingleThreadProxy.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;

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

    RefPtr<CCLayerImpl> owningLayer = CCLayerImpl::create(0);
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

    RefPtr<CCLayerImpl> dummyMask = CCLayerImpl::create(1);
    TransformationMatrix dummyMatrix;
    dummyMatrix.translate(1.0, 2.0);

    // The rest of the surface properties are either internal and should not cause change,
    // or they are already accounted for by the owninglayer->layerPropertyChanged().
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawOpacity(0.5));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setReplicaDrawTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setOriginTransform(dummyMatrix));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setSkipsDraw(true));
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->clearLayerList());
    EXECUTE_AND_VERIFY_SURFACE_DID_NOT_CHANGE(renderSurface->setMaskLayer(dummyMask.get()));
}

} // namespace
