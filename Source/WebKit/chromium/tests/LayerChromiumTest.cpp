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

#include "LayerChromium.h"

#include "cc/CCLayerTreeHost.h"
#include "CCLayerTreeTestCommon.h"
#include "LayerPainterChromium.h"
#include "NonCompositedContentHost.h"
#include "WebCompositor.h"
#include "cc/CCLayerTreeHost.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKitTests;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

#define EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(numTimesExpectedSetNeedsCommit, codeToTest) do { \
        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times((numTimesExpectedSetNeedsCommit));      \
        codeToTest;                                                                                   \
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());                                      \
    } while (0)

namespace {

class FakeCCLayerTreeHostClient : public CCLayerTreeHostClient {
public:
    virtual void animateAndLayout(double frameBeginTime) { }
    virtual void applyScrollAndScale(const IntSize& scrollDelta, float pageScale) { }
    virtual PassRefPtr<GraphicsContext3D> createLayerTreeHostContext3D() { return 0; }
    virtual void didRecreateGraphicsContext(bool success) { }
    virtual void didCommitAndDrawFrame() { }
    virtual void didCompleteSwapBuffers() { }

    // Used only in the single-threaded path.
    virtual void scheduleComposite() { }
};

class MockCCLayerTreeHost : public CCLayerTreeHost {
public:
    MockCCLayerTreeHost()
        : CCLayerTreeHost(&m_fakeClient, CCSettings())
    {
        initialize();
    }

    MOCK_METHOD0(setNeedsCommit, void());

private:
    FakeCCLayerTreeHostClient m_fakeClient;
};

class MockLayerPainterChromium : public LayerPainterChromium {
public:
    virtual void paint(GraphicsContext&, const IntRect&) { }
};


class LayerChromiumTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        // Initialize without threading support.
        WebKit::WebCompositor::initialize(0);
        m_layerTreeHost = adoptRef(new MockCCLayerTreeHost);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
        m_parent.clear();
        m_child1.clear();
        m_child2.clear();
        m_child3.clear();
        m_grandChild1.clear();
        m_grandChild2.clear();
        m_grandChild3.clear();

        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();
        WebKit::WebCompositor::shutdown();
    }

    void verifyTestTreeInitialState() const
    {
        ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
        EXPECT_EQ(m_child1, m_parent->children()[0]);
        EXPECT_EQ(m_child2, m_parent->children()[1]);
        EXPECT_EQ(m_child3, m_parent->children()[2]);
        EXPECT_EQ(m_parent.get(), m_child1->parent());
        EXPECT_EQ(m_parent.get(), m_child2->parent());
        EXPECT_EQ(m_parent.get(), m_child3->parent());

        ASSERT_EQ(static_cast<size_t>(2), m_child1->children().size());
        EXPECT_EQ(m_grandChild1, m_child1->children()[0]);
        EXPECT_EQ(m_grandChild2, m_child1->children()[1]);
        EXPECT_EQ(m_child1.get(), m_grandChild1->parent());
        EXPECT_EQ(m_child1.get(), m_grandChild2->parent());

        ASSERT_EQ(static_cast<size_t>(1), m_child2->children().size());
        EXPECT_EQ(m_grandChild3, m_child2->children()[0]);
        EXPECT_EQ(m_child2.get(), m_grandChild3->parent());

        ASSERT_EQ(static_cast<size_t>(0), m_child3->children().size());
    }

    void createSimpleTestTree()
    {
        m_parent = LayerChromium::create(0);
        m_child1 = LayerChromium::create(0);
        m_child2 = LayerChromium::create(0);
        m_child3 = LayerChromium::create(0);
        m_grandChild1 = LayerChromium::create(0);
        m_grandChild2 = LayerChromium::create(0);
        m_grandChild3 = LayerChromium::create(0);

        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
        m_layerTreeHost->setRootLayer(m_parent);

        m_parent->addChild(m_child1);
        m_parent->addChild(m_child2);
        m_parent->addChild(m_child3);
        m_child1->addChild(m_grandChild1);
        m_child1->addChild(m_grandChild2);
        m_child2->addChild(m_grandChild3);

        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

        verifyTestTreeInitialState();
    }

    RefPtr<MockCCLayerTreeHost> m_layerTreeHost;
    RefPtr<LayerChromium> m_parent, m_child1, m_child2, m_child3, m_grandChild1, m_grandChild2, m_grandChild3;
};

TEST_F(LayerChromiumTest, basicCreateAndDestroy)
{
    RefPtr<LayerChromium> testLayer = LayerChromium::create(0);
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
}

TEST_F(LayerChromiumTest, addAndRemoveChild)
{
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child = LayerChromium::create(0);

    // Upon creation, layers should not have children or parent.
    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());
    EXPECT_FALSE(child->parent());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, m_layerTreeHost->setRootLayer(parent));

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->addChild(child));

    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child.get(), parent->children()[0]);
    EXPECT_EQ(parent.get(), child->parent());
    EXPECT_EQ(parent.get(), child->rootLayer());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), child->removeFromParent());
}

TEST_F(LayerChromiumTest, insertChild)
{
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child1 = LayerChromium::create(0);
    RefPtr<LayerChromium> child2 = LayerChromium::create(0);
    RefPtr<LayerChromium> child3 = LayerChromium::create(0);
    RefPtr<LayerChromium> child4 = LayerChromium::create(0);

    parent->setLayerTreeHost(m_layerTreeHost.get());

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    // Case 1: inserting to empty list.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child3, 0));
    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child3, parent->children()[0]);
    EXPECT_EQ(parent.get(), child3->parent());

    // Case 2: inserting to beginning of list
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child1, 0));
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child3, parent->children()[1]);
    EXPECT_EQ(parent.get(), child1->parent());

    // Case 3: inserting to middle of list
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child2, 1));
    ASSERT_EQ(static_cast<size_t>(3), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(parent.get(), child2->parent());

    // Case 4: inserting to end of list
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child4, 3));

    ASSERT_EQ(static_cast<size_t>(4), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(child4, parent->children()[3]);
    EXPECT_EQ(parent.get(), child4->parent());

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, insertChildPastEndOfList)
{
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child1 = LayerChromium::create(0);
    RefPtr<LayerChromium> child2 = LayerChromium::create(0);

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    // insert to an out-of-bounds index
    parent->insertChild(child1, 53);

    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);

    // insert another child to out-of-bounds, when list is not already empty.
    parent->insertChild(child2, 2459);

    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
}

TEST_F(LayerChromiumTest, insertSameChildTwice)
{
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child1 = LayerChromium::create(0);
    RefPtr<LayerChromium> child2 = LayerChromium::create(0);

    parent->setLayerTreeHost(m_layerTreeHost.get());

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child1, 0));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child2, 1));

    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);

    // Inserting the same child again should cause the child to be removed and re-inserted at the new location.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), parent->insertChild(child1, 1));

    // child1 should now be at the end of the list.
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child2, parent->children()[0]);
    EXPECT_EQ(child1, parent->children()[1]);

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, replaceChildWithNewChild)
{
    createSimpleTestTree();
    RefPtr<LayerChromium> child4 = LayerChromium::create(0);

    EXPECT_FALSE(child4->parent());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerChromiumTest, replaceChildWithNewChildThatHasOtherParent)
{
    createSimpleTestTree();

    // create another simple tree with testLayer and child4.
    RefPtr<LayerChromium> testLayer = LayerChromium::create(0);
    RefPtr<LayerChromium> child4 = LayerChromium::create(0);
    testLayer->addChild(child4);
    ASSERT_EQ(static_cast<size_t>(1), testLayer->children().size());
    EXPECT_EQ(child4, testLayer->children()[0]);
    EXPECT_EQ(testLayer.get(), child4->parent());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    // testLayer should no longer have child4,
    // and child2 should no longer have a parent.
    ASSERT_EQ(static_cast<size_t>(0), testLayer->children().size());
    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerChromiumTest, replaceChildWithSameChild)
{
    createSimpleTestTree();

    // setNeedsCommit should not be called because its the same child
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, m_parent->replaceChild(m_child2.get(), m_child2));

    verifyTestTreeInitialState();
}

TEST_F(LayerChromiumTest, removeAllChildren)
{
    createSimpleTestTree();

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(3), m_parent->removeAllChildren());

    ASSERT_EQ(static_cast<size_t>(0), m_parent->children().size());
    EXPECT_FALSE(m_child1->parent());
    EXPECT_FALSE(m_child2->parent());
    EXPECT_FALSE(m_child3->parent());
}

TEST_F(LayerChromiumTest, setChildren)
{
    RefPtr<LayerChromium> oldParent = LayerChromium::create(0);
    RefPtr<LayerChromium> newParent = LayerChromium::create(0);

    RefPtr<LayerChromium> child1 = LayerChromium::create(0);
    RefPtr<LayerChromium> child2 = LayerChromium::create(0);

    Vector<RefPtr<LayerChromium> > newChildren;
    newChildren.append(child1);
    newChildren.append(child2);

    // Set up and verify initial test conditions: child1 has a parent, child2 has no parent.
    oldParent->addChild(child1);
    ASSERT_EQ(static_cast<size_t>(0), newParent->children().size());
    EXPECT_EQ(oldParent.get(), child1->parent());
    EXPECT_FALSE(child2->parent());

    newParent->setLayerTreeHost(m_layerTreeHost.get());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), newParent->setChildren(newChildren));

    ASSERT_EQ(static_cast<size_t>(2), newParent->children().size());
    EXPECT_EQ(newParent.get(), child1->parent());
    EXPECT_EQ(newParent.get(), child2->parent());

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, getRootLayerAfterTreeManipulations)
{
    createSimpleTestTree();

    // For this test we don't care about setNeedsCommit calls.
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));

    RefPtr<LayerChromium> child4 = LayerChromium::create(0);

    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(child4.get(),   child4->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_child1->removeFromParent();

    // child1 and its children, grandChild1 and grandChild2 are now on a separate subtree.
    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(child4.get(), child4->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_grandChild3->addChild(child4);

    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(m_parent.get(), child4->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_child2->replaceChild(m_grandChild3.get(), m_child1);

    // grandChild3 gets orphaned and the child1 subtree gets planted back into the tree under child2.
    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(m_grandChild3.get(), child4->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_grandChild3.get(), m_grandChild3->rootLayer());
}

// FIXME: need to add a test for getDescendantDrawsContent after resolving
//        some issues with LayerChromium::descendantDrawsContent().
//        see discussion on issue 67750

TEST_F(LayerChromiumTest, checkSetNeedsDisplayCausesCorrectBehavior)
{
    // The semantics for setNeedsDisplay which are tested here:
    //   1. sets needsDisplay flag appropriately.
    //   2. indirectly calls setNeedsCommit, exactly once for each call to setNeedsDisplay.

    RefPtr<LayerChromium> testLayer = LayerChromium::create(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());

    IntSize testBounds = IntSize(501, 508);

    FloatRect dirty1 = FloatRect(10.0f, 15.0f, 1.0f, 2.0f);
    FloatRect dirty2 = FloatRect(20.0f, 25.0f, 3.0f, 4.0f);
    FloatRect emptyDirtyRect = FloatRect(40.0f, 45.0f, 0, 0);
    FloatRect outOfBoundsDirtyRect = FloatRect(400.0f, 405.0f, 500.0f, 502.0f);

    // Before anything, testLayer should not be dirty.
    EXPECT_FALSE(testLayer->needsDisplay());

    // This is just initialization, but setNeedsCommit behavior is verified anyway to avoid warnings.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));
    testLayer = LayerChromium::create(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_FALSE(testLayer->needsDisplay());

    // The real test begins here.

    // Case 1: needsDisplay flag should not change because of an empty dirty rect.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(emptyDirtyRect));
    EXPECT_FALSE(testLayer->needsDisplay());

    // Case 2: basic.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(dirty1));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 3: a second dirty rect.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(dirty2));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 4: LayerChromium should accept dirty rects that go beyond its bounds.
    testLayer = LayerChromium::create(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(outOfBoundsDirtyRect));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 5: setNeedsDisplay() without the dirty rect arg.
    testLayer = LayerChromium::create(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplay());
    EXPECT_TRUE(testLayer->needsDisplay());
}

TEST_F(LayerChromiumTest, checkSetNeedsDisplayWithNullDelegate)
{
    RefPtr<LayerChromium> testLayer = LayerChromium::create(0);
    IntSize testBounds = IntSize(501, 508);

    FloatRect dirty = FloatRect(10.0f, 15.0f, 1.0f, 2.0f);

    testLayer->setBounds(testBounds);
    EXPECT_TRUE(testLayer->needsDisplay());

    testLayer = LayerChromium::create(0);
    EXPECT_FALSE(testLayer->needsDisplay());

    testLayer->setNeedsDisplayRect(dirty);
    EXPECT_TRUE(testLayer->needsDisplay());
}

TEST_F(LayerChromiumTest, checkPropertyChangeCausesCorrectBehavior)
{
    RefPtr<LayerChromium> testLayer = LayerChromium::create(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());

    RefPtr<LayerChromium> dummyLayer = LayerChromium::create(0); // just a dummy layer for this test case.

    // sanity check of initial test condition
    EXPECT_FALSE(testLayer->needsDisplay());

    // Test properties that should not call needsDisplay and needsCommit when changed.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setName("Test Layer"));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setVisibleLayerRect(IntRect(0, 0, 40, 50)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setUsesLayerClipping(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setIsNonCompositedContent(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setDrawOpacity(0.5f));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setClipRect(IntRect(3, 3, 8, 8)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setTargetRenderSurface(0));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setDrawTransform(TransformationMatrix()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setScreenSpaceTransform(TransformationMatrix()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setDrawableContentRect(IntRect(4, 5, 6, 7)));
    EXPECT_FALSE(testLayer->needsDisplay());

    // Next, test properties that should call setNeedsCommit (but not setNeedsDisplay)
    // All properties need to be set to new values in order for setNeedsCommit to be called.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setAnchorPoint(FloatPoint(1.23f, 4.56f)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setAnchorPointZ(0.7f));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBackgroundColor(Color(0.4f, 0.4f, 0.4f)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setMasksToBounds(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setMaskLayer(dummyLayer.get()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setOpacity(0.5f));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setOpaque(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setPosition(FloatPoint(4.0f, 9.0f)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setReplicaLayer(dummyLayer.get()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setSublayerTransform(TransformationMatrix(0, 0, 0, 0, 0, 0)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setScrollable(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setScrollPosition(IntPoint(10, 10)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setTransform(TransformationMatrix(0, 0, 0, 0, 0, 0)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setDoubleSided(false));

    // The above tests should not have caused a change to the needsDisplay flag.
    EXPECT_FALSE(testLayer->needsDisplay());

    // Test properties that should call setNeedsDisplay and setNeedsCommit
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(IntSize(5, 10)));
    EXPECT_TRUE(testLayer->needsDisplay());
}

class LayerChromiumWithContentScaling : public LayerChromium {
public:
    explicit LayerChromiumWithContentScaling(CCLayerDelegate* delegate)
        : LayerChromium(delegate)
    {
    }

    virtual bool needsContentsScale() const
    {
        return true;
    }

    void resetNeedsDisplay()
    {
        m_needsDisplay = false;
    }
};

TEST_F(LayerChromiumTest, checkContentsScaleChangeTriggersNeedsDisplay)
{
    RefPtr<LayerChromiumWithContentScaling> testLayer = adoptRef(new LayerChromiumWithContentScaling(0));
    testLayer->setLayerTreeHost(m_layerTreeHost.get());

    IntSize testBounds = IntSize(320, 240);
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));

    testLayer->resetNeedsDisplay();
    EXPECT_FALSE(testLayer->needsDisplay());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setContentsScale(testLayer->contentsScale() + 1.f));
    EXPECT_TRUE(testLayer->needsDisplay());
}

class FakeCCLayerTreeHost : public CCLayerTreeHost {
public:
    static PassRefPtr<FakeCCLayerTreeHost> create()
    {
        RefPtr<FakeCCLayerTreeHost> host = adoptRef(new FakeCCLayerTreeHost);
        // The initialize call will fail, since our client doesn't provide a valid GraphicsContext3D, but it doesn't matter in the tests that use this fake so ignore the return value.
        host->initialize();
        return host.release();
    }

private:
    FakeCCLayerTreeHost()
        : CCLayerTreeHost(&m_client, CCSettings())
    {
    }

    FakeCCLayerTreeHostClient m_client;
};

void assertLayerTreeHostMatchesForSubtree(LayerChromium* layer, CCLayerTreeHost* host)
{
    EXPECT_EQ(host, layer->layerTreeHost());

    for (size_t i = 0; i < layer->children().size(); ++i)
        assertLayerTreeHostMatchesForSubtree(layer->children()[i].get(), host);

    if (layer->maskLayer())
        assertLayerTreeHostMatchesForSubtree(layer->maskLayer(), host);

    if (layer->replicaLayer())
        assertLayerTreeHostMatchesForSubtree(layer->replicaLayer(), host);
}


TEST(LayerChromiumLayerTreeHostTest, enteringTree)
{
    WebKit::WebCompositor::initialize(0);
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child = LayerChromium::create(0);
    RefPtr<LayerChromium> mask = LayerChromium::create(0);
    RefPtr<LayerChromium> replica = LayerChromium::create(0);
    RefPtr<LayerChromium> replicaMask = LayerChromium::create(0);

    // Set up a detached tree of layers. The host pointer should be nil for these layers.
    parent->addChild(child);
    child->setMaskLayer(mask.get());
    child->setReplicaLayer(replica.get());
    replica->setMaskLayer(mask.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);

    RefPtr<FakeCCLayerTreeHost> layerTreeHost = FakeCCLayerTreeHost::create();
    // Setting the root layer should set the host pointer for all layers in the tree.
    layerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Clearing the root layer should also clear out the host pointers for all layers in the tree.
    layerTreeHost->setRootLayer(0);

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);

    layerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(LayerChromiumLayerTreeHostTest, addingLayerSubtree)
{
    WebKit::WebCompositor::initialize(0);
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<FakeCCLayerTreeHost> layerTreeHost = FakeCCLayerTreeHost::create();

    layerTreeHost->setRootLayer(parent.get());

    EXPECT_EQ(parent->layerTreeHost(), layerTreeHost.get());

    // Adding a subtree to a layer already associated with a host should set the host pointer on all layers in that subtree.
    RefPtr<LayerChromium> child = LayerChromium::create(0);
    RefPtr<LayerChromium> grandChild = LayerChromium::create(0);
    child->addChild(grandChild);

    // Masks, replicas, and replica masks should pick up the new host too.
    RefPtr<LayerChromium> childMask = LayerChromium::create(0);
    child->setMaskLayer(childMask.get());
    RefPtr<LayerChromium> childReplica = LayerChromium::create(0);
    child->setReplicaLayer(childReplica.get());
    RefPtr<LayerChromium> childReplicaMask = LayerChromium::create(0);
    childReplica->setMaskLayer(childReplicaMask.get());

    parent->addChild(child);
    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    layerTreeHost->setRootLayer(0);
    layerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(LayerChromiumLayerTreeHostTest, changeHost)
{
    WebKit::WebCompositor::initialize(0);
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> child = LayerChromium::create(0);
    RefPtr<LayerChromium> mask = LayerChromium::create(0);
    RefPtr<LayerChromium> replica = LayerChromium::create(0);
    RefPtr<LayerChromium> replicaMask = LayerChromium::create(0);

    // Same setup as the previous test.
    parent->addChild(child);
    child->setMaskLayer(mask.get());
    child->setReplicaLayer(replica.get());
    replica->setMaskLayer(mask.get());

    RefPtr<FakeCCLayerTreeHost> firstLayerTreeHost = FakeCCLayerTreeHost::create();
    firstLayerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), firstLayerTreeHost.get());

    // Now re-root the tree to a new host (simulating what we do on a context lost event).
    // This should update the host pointers for all layers in the tree.
    RefPtr<FakeCCLayerTreeHost> secondLayerTreeHost = FakeCCLayerTreeHost::create();
    secondLayerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), secondLayerTreeHost.get());

    secondLayerTreeHost->setRootLayer(0);
    firstLayerTreeHost.clear();
    secondLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(LayerChromiumLayerTreeHostTest, changeHostInSubtree)
{
    WebKit::WebCompositor::initialize(0);
    RefPtr<LayerChromium> firstParent = LayerChromium::create(0);
    RefPtr<LayerChromium> firstChild = LayerChromium::create(0);
    RefPtr<LayerChromium> secondParent = LayerChromium::create(0);
    RefPtr<LayerChromium> secondChild = LayerChromium::create(0);
    RefPtr<LayerChromium> secondGrandChild = LayerChromium::create(0);

    // First put all children under the first parent and set the first host.
    firstParent->addChild(firstChild);
    secondChild->addChild(secondGrandChild);
    firstParent->addChild(secondChild);

    RefPtr<FakeCCLayerTreeHost> firstLayerTreeHost = FakeCCLayerTreeHost::create();
    firstLayerTreeHost->setRootLayer(firstParent.get());

    assertLayerTreeHostMatchesForSubtree(firstParent.get(), firstLayerTreeHost.get());

    // Now reparent the subtree starting at secondChild to a layer in a different tree.
    RefPtr<FakeCCLayerTreeHost> secondLayerTreeHost = FakeCCLayerTreeHost::create();
    secondLayerTreeHost->setRootLayer(secondParent.get());

    secondParent->addChild(secondChild);

    // The moved layer and its children should point to the new host.
    EXPECT_EQ(secondLayerTreeHost.get(), secondChild->layerTreeHost());
    EXPECT_EQ(secondLayerTreeHost.get(), secondGrandChild->layerTreeHost());

    // Test over, cleanup time.
    firstLayerTreeHost->setRootLayer(0);
    secondLayerTreeHost->setRootLayer(0);
    firstLayerTreeHost.clear();
    secondLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(LayerChromiumLayerTreeHostTest, replaceMaskAndReplicaLayer)
{
    WebKit::WebCompositor::initialize(0);
    RefPtr<LayerChromium> parent = LayerChromium::create(0);
    RefPtr<LayerChromium> mask = LayerChromium::create(0);
    RefPtr<LayerChromium> replica = LayerChromium::create(0);
    RefPtr<LayerChromium> maskChild = LayerChromium::create(0);
    RefPtr<LayerChromium> replicaChild = LayerChromium::create(0);
    RefPtr<LayerChromium> maskReplacement = LayerChromium::create(0);
    RefPtr<LayerChromium> replicaReplacement = LayerChromium::create(0);

    parent->setMaskLayer(mask.get());
    parent->setReplicaLayer(replica.get());
    mask->addChild(maskChild);
    replica->addChild(replicaChild);

    RefPtr<FakeCCLayerTreeHost> layerTreeHost = FakeCCLayerTreeHost::create();
    layerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Replacing the mask should clear out the old mask's subtree's host pointers.
    parent->setMaskLayer(maskReplacement.get());
    EXPECT_EQ(0, mask->layerTreeHost());
    EXPECT_EQ(0, maskChild->layerTreeHost());

    // Same for replacing a replica layer.
    parent->setReplicaLayer(replicaReplacement.get());
    EXPECT_EQ(0, replica->layerTreeHost());
    EXPECT_EQ(0, replicaChild->layerTreeHost());

    // Test over, cleanup time.
    layerTreeHost->setRootLayer(0);
    layerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

} // namespace
