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

#include "TreeSynchronizer.h"

#include "LayerChromium.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCProxy.h"
#include "cc/CCSingleThreadProxy.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class MockCCLayerImpl : public CCLayerImpl {
public:
    static PassRefPtr<MockCCLayerImpl> create(int layerId)
    {
        return adoptRef(new MockCCLayerImpl(layerId));
    }
    virtual ~MockCCLayerImpl()
    {
        if (m_ccLayerDestructionList)
            m_ccLayerDestructionList->append(id());
    }

    void setCCLayerDestructionList(Vector<int>* list) { m_ccLayerDestructionList = list; }

private:
    MockCCLayerImpl(int layerId)
        : CCLayerImpl(layerId)
        , m_ccLayerDestructionList(0)
    {
    }

    Vector<int>* m_ccLayerDestructionList;
};

class MockLayerChromium : public LayerChromium {
public:
    static PassRefPtr<MockLayerChromium> create(Vector<int>* ccLayerDestructionList)
    {
        return adoptRef(new MockLayerChromium(ccLayerDestructionList));
    }

    virtual ~MockLayerChromium() { }

    virtual PassRefPtr<CCLayerImpl> createCCLayerImpl()
    {
        return MockCCLayerImpl::create(m_layerId);
    }

    virtual void pushPropertiesTo(CCLayerImpl* ccLayer)
    {
        LayerChromium::pushPropertiesTo(ccLayer);

        MockCCLayerImpl* mockCCLayer = static_cast<MockCCLayerImpl*>(ccLayer);
        mockCCLayer->setCCLayerDestructionList(m_ccLayerDestructionList);
    }
private:
    MockLayerChromium(Vector<int>* ccLayerDestructionList)
        : LayerChromium()
        , m_ccLayerDestructionList(ccLayerDestructionList)
    {
    }

    Vector<int>* m_ccLayerDestructionList;
};

void expectTreesAreIdentical(LayerChromium* layer, CCLayerImpl* ccLayer)
{
    ASSERT_TRUE(layer);
    ASSERT_TRUE(ccLayer);

    EXPECT_EQ(layer->id(), ccLayer->id());

    ASSERT_EQ(!!layer->maskLayer(), !!ccLayer->maskLayer());
    if (layer->maskLayer())
        expectTreesAreIdentical(layer->maskLayer(), ccLayer->maskLayer());

    ASSERT_EQ(!!layer->replicaLayer(), !!ccLayer->replicaLayer());
    if (layer->replicaLayer())
        expectTreesAreIdentical(layer->replicaLayer(), ccLayer->replicaLayer());

    const Vector<RefPtr<LayerChromium> >& layerChildren = layer->children();
    const Vector<RefPtr<CCLayerImpl> >& ccLayerChildren = ccLayer->children();

    ASSERT_EQ(layerChildren.size(), ccLayerChildren.size());

    for (size_t i = 0; i < layerChildren.size(); ++i)
        expectTreesAreIdentical(layerChildren[i].get(), ccLayerChildren[i].get());
}

// Constructs a very simple tree and synchronizes it without trying to reuse any preexisting layers.
TEST(TreeSynchronizerTest, syncSimpleTreeFromEmpty)
{
    DebugScopedSetImplThread impl;
    RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());

    RefPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), 0);

    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());
}

// Constructs a very simple tree and synchronizes it attempting to reuse some layers
TEST(TreeSynchronizerTest, syncSimpleTreeReusingLayers)
{
    DebugScopedSetImplThread impl;
    Vector<int> ccLayerDestructionList;

    RefPtr<LayerChromium> layerTreeRoot = MockLayerChromium::create(&ccLayerDestructionList);
    layerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    layerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    RefPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), 0);
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    // Add a new layer to the LayerChromium side
    layerTreeRoot->children()[0]->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    // Remove one.
    layerTreeRoot->children()[1]->removeFromParent();
    int secondCCLayerId = ccLayerTreeRoot->children()[1]->id();

    // Synchronize again. After the sync the trees should be equivalent and we should have created and destroyed one CCLayerImpl.
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.release());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    ASSERT_EQ(1u, ccLayerDestructionList.size());
    EXPECT_EQ(secondCCLayerId, ccLayerDestructionList[0]);
}

TEST(TreeSynchronizerTest, syncSimpleTreeAndProperties)
{
    DebugScopedSetImplThread impl;
    RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());

    // Pick some random properties to set. The values are not important, we're just testing that at least some properties are making it through.
    FloatPoint rootPosition = FloatPoint(2.3, 7.4);
    layerTreeRoot->setPosition(rootPosition);

    float firstChildOpacity = 0.25;
    layerTreeRoot->children()[0]->setOpacity(firstChildOpacity);

    IntSize secondChildBounds = IntSize(25, 53);
    layerTreeRoot->children()[1]->setBounds(secondChildBounds);

    RefPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), 0);
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    // Check that the property values we set on the LayerChromium tree are reflected in the CCLayerImpl tree.
    FloatPoint rootCCLayerPosition = ccLayerTreeRoot->position();
    EXPECT_EQ(rootPosition.x(), rootCCLayerPosition.x());
    EXPECT_EQ(rootPosition.y(), rootCCLayerPosition.y());

    EXPECT_EQ(firstChildOpacity, ccLayerTreeRoot->children()[0]->opacity());

    IntSize secondCCLayerChildBounds = ccLayerTreeRoot->children()[1]->bounds();
    EXPECT_EQ(secondChildBounds.width(), secondCCLayerChildBounds.width());
    EXPECT_EQ(secondChildBounds.height(), secondCCLayerChildBounds.height());
}

TEST(TreeSynchronizerTest, reuseCCLayersAfterStructuralChange)
{
    DebugScopedSetImplThread impl;
    Vector<int> ccLayerDestructionList;

    // Set up a tree with this sort of structure:
    // root --- A --- B ---+--- C
    //                     |
    //                     +--- D
    RefPtr<LayerChromium> layerTreeRoot = MockLayerChromium::create(&ccLayerDestructionList);
    layerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    RefPtr<LayerChromium> layerA = layerTreeRoot->children()[0].get();
    layerA->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    RefPtr<LayerChromium> layerB = layerA->children()[0].get();
    layerB->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    RefPtr<LayerChromium> layerC = layerB->children()[0].get();
    layerB->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    RefPtr<LayerChromium> layerD = layerB->children()[1].get();

    RefPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), 0);
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    // Now restructure the tree to look like this:
    // root --- D ---+--- A
    //               |
    //               +--- C --- B
    layerTreeRoot->removeAllChildren();
    layerD->removeAllChildren();
    layerTreeRoot->addChild(layerD);
    layerA->removeAllChildren();
    layerD->addChild(layerA);
    layerC->removeAllChildren();
    layerD->addChild(layerC);
    layerB->removeAllChildren();
    layerC->addChild(layerB);

    // After another synchronize our trees should match and we should not have destroyed any CCLayerImpls
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.release());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    EXPECT_EQ(0u, ccLayerDestructionList.size());
}

// Constructs a very simple tree, synchronizes it, then synchronizes to a totally new tree. All layers from the old tree should be deleted.
TEST(TreeSynchronizerTest, syncSimpleTreeThenDestroy)
{
    DebugScopedSetImplThread impl;
    Vector<int> ccLayerDestructionList;

    RefPtr<LayerChromium> oldLayerTreeRoot = MockLayerChromium::create(&ccLayerDestructionList);
    oldLayerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    oldLayerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    int oldTreeRootLayerId = oldLayerTreeRoot->id();
    int oldTreeFirstChildLayerId = oldLayerTreeRoot->children()[0]->id();
    int oldTreeSecondChildLayerId = oldLayerTreeRoot->children()[1]->id();

    RefPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(oldLayerTreeRoot.get(), 0);
    expectTreesAreIdentical(oldLayerTreeRoot.get(), ccLayerTreeRoot.get());

    // Remove all children on the LayerChromium side.
    oldLayerTreeRoot->removeAllChildren();

    // Synchronize again. After the sync all CCLayerImpls from the old tree should be deleted.
    RefPtr<LayerChromium> newLayerTreeRoot = LayerChromium::create();
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(newLayerTreeRoot.get(), ccLayerTreeRoot.release());
    expectTreesAreIdentical(newLayerTreeRoot.get(), ccLayerTreeRoot.get());

    ASSERT_EQ(3u, ccLayerDestructionList.size());
    EXPECT_TRUE(ccLayerDestructionList.contains(oldTreeRootLayerId));
    EXPECT_TRUE(ccLayerDestructionList.contains(oldTreeFirstChildLayerId));
    EXPECT_TRUE(ccLayerDestructionList.contains(oldTreeSecondChildLayerId));
}

// Constructs+syncs a tree with mask, replica, and replica mask layers.
TEST(TreeSynchronizerTest, syncMaskReplicaAndReplicaMaskLayers)
{
    DebugScopedSetImplThread impl;
    RefPtr<LayerChromium> layerTreeRoot = LayerChromium::create();
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());

    // First child gets a mask layer.
    RefPtr<LayerChromium> maskLayer = LayerChromium::create();
    layerTreeRoot->children()[0]->setMaskLayer(maskLayer.get());

    // Second child gets a replica layer.
    RefPtr<LayerChromium> replicaLayer = LayerChromium::create();
    layerTreeRoot->children()[1]->setReplicaLayer(replicaLayer.get());

    // Third child gets a replica layer with a mask layer.
    RefPtr<LayerChromium> replicaLayerWithMask = LayerChromium::create();
    RefPtr<LayerChromium> replicaMaskLayer = LayerChromium::create();
    replicaLayerWithMask->setMaskLayer(replicaMaskLayer.get());
    layerTreeRoot->children()[2]->setReplicaLayer(replicaLayerWithMask.get());

    RefPtr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), 0);

    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    // Remove the mask layer.
    layerTreeRoot->children()[0]->setMaskLayer(0);
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    // Remove the replica layer.
    layerTreeRoot->children()[1]->setReplicaLayer(0);
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());

    // Remove the replica mask.
    replicaLayerWithMask->setMaskLayer(0);
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get());
}


} // namespace
