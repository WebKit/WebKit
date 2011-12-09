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

#include "CCLayerTreeTestCommon.h"
#include "LayerPainterChromium.h"
#include "NonCompositedContentHost.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKitTests;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

#define EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(delegate, numTimesExpectedNotifySync, codeToTest) \
    EXPECT_CALL((delegate), notifySyncRequired()).Times((numTimesExpectedNotifySync)); \
    codeToTest;                                                         \
    Mock::VerifyAndClearExpectations(&(delegate))

namespace {

class MockLayerDelegate : public CCLayerDelegate {
public:
    MOCK_CONST_METHOD0(drawsContent, bool());
    MOCK_CONST_METHOD0(preserves3D, bool());
    MOCK_METHOD2(paintContents, void(GraphicsContext&, const IntRect&));
    MOCK_METHOD0(notifySyncRequired, void());
};

class MockLayerPainterChromium : public LayerPainterChromium {
public:
    virtual void paint(GraphicsContext&, const IntRect&) { }
};

class MockNonCompositedContentHost : public NonCompositedContentHost {
public:
    static PassOwnPtr<MockNonCompositedContentHost> create()
    {
        return adoptPtr(new MockNonCompositedContentHost);
    }

    MOCK_METHOD1(notifySyncRequired, void(const GraphicsLayer*));

private:
    MockNonCompositedContentHost()
        : NonCompositedContentHost(adoptPtr(new MockLayerPainterChromium()))
    {
        m_scrollLayer = GraphicsLayer::create(0);
        setScrollLayer(m_scrollLayer.get());
    }
    OwnPtr<GraphicsLayer> m_scrollLayer;
};

class LayerChromiumWithInstrumentedDestructor : public LayerChromium {
public:
    explicit LayerChromiumWithInstrumentedDestructor(CCLayerDelegate* delegate)
        : LayerChromium(delegate)
    {
    }

    virtual ~LayerChromiumWithInstrumentedDestructor()
    {
        s_numInstancesDestroyed++;
    }

    static int getNumInstancesDestroyed() { return s_numInstancesDestroyed; }
    static void resetNumInstancesDestroyed() { s_numInstancesDestroyed = 0; }

private:
    static int s_numInstancesDestroyed;
};

int LayerChromiumWithInstrumentedDestructor::s_numInstancesDestroyed = 0;

class LayerChromiumTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        // m_silentDelegate is initialized to be just a stub and will
        // not print any warnings. It is used when we are not worried
        // about testing how the delegate is called.
        EXPECT_CALL(m_silentDelegate, drawsContent()).Times(AnyNumber());
        EXPECT_CALL(m_silentDelegate, preserves3D()).Times(AnyNumber());
        EXPECT_CALL(m_silentDelegate, paintContents(_, _)).Times(AnyNumber());
        EXPECT_CALL(m_silentDelegate, notifySyncRequired()).Times(AnyNumber());

        // Static variables need to be reset for every new test case
        LayerChromiumWithInstrumentedDestructor::resetNumInstancesDestroyed();
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
        m_parent = LayerChromium::create(&m_parentDelegate);
        m_child1 = LayerChromium::create(&m_silentDelegate);
        m_child2 = LayerChromium::create(&m_silentDelegate);
        m_child3 = LayerChromium::create(&m_silentDelegate);
        m_grandChild1 = LayerChromium::create(&m_silentDelegate);
        m_grandChild2 = LayerChromium::create(&m_silentDelegate);
        m_grandChild3 = LayerChromium::create(&m_silentDelegate);

        EXPECT_CALL(m_parentDelegate, notifySyncRequired()).Times(3);
        m_parent->addChild(m_child1);
        m_parent->addChild(m_child2);
        m_parent->addChild(m_child3);
        Mock::VerifyAndClearExpectations(&m_parentDelegate);
        m_child1->addChild(m_grandChild1);
        m_child1->addChild(m_grandChild2);
        m_child2->addChild(m_grandChild3);

        verifyTestTreeInitialState();
    }

    MockLayerDelegate m_silentDelegate, m_parentDelegate;
    RefPtr<LayerChromium> m_parent, m_child1, m_child2, m_child3, m_grandChild1, m_grandChild2, m_grandChild3;
};

TEST_F(LayerChromiumTest, basicCreateAndDestroy)
{
    MockLayerDelegate mockDelegate;

    // notifySyncRequired should not be called just because the layer is created or destroyed.
    EXPECT_CALL(mockDelegate, notifySyncRequired()).Times(0);

    RefPtr<LayerChromiumWithInstrumentedDestructor> testLayer = adoptRef(new LayerChromiumWithInstrumentedDestructor(&mockDelegate));
    ASSERT_TRUE(testLayer.get());

    // notifySyncRequired should also not be called on the destructor when the layer has no children.
    // so we need to make sure the layer is destroyed before the mock delegate.
    ASSERT_EQ(0, LayerChromiumWithInstrumentedDestructor::getNumInstancesDestroyed());
    testLayer.release();
    ASSERT_EQ(1, LayerChromiumWithInstrumentedDestructor::getNumInstancesDestroyed());
}

TEST_F(LayerChromiumTest, addAndRemoveChild)
{
    MockLayerDelegate parentDelegate;
    MockLayerDelegate childDelegate;
    RefPtr<LayerChromium> parent = LayerChromium::create(&parentDelegate);
    RefPtr<LayerChromium> child = LayerChromium::create(&childDelegate);

    // Upon creation, layers should not have children or parent.
    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());
    EXPECT_FALSE(child->parent());

    // Parent calls notifySyncRequired exactly once when adding child.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->addChild(child));

    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child.get(), parent->children()[0]);
    EXPECT_EQ(parent.get(), child->parent());
    EXPECT_EQ(parent.get(), child->rootLayer());

    // removeFromParent should cause the parent's notifySyncRequired to be called exactly once.
    // The childDelegate notifySyncRequired should remain un-used.
    EXPECT_CALL(childDelegate, notifySyncRequired()).Times(0);
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, child->removeFromParent());
}

TEST_F(LayerChromiumTest, verifyDestructorSemantics)
{
    MockLayerDelegate parentDelegate;
    MockLayerDelegate childDelegate;
    RefPtr<LayerChromiumWithInstrumentedDestructor> parent = adoptRef(new LayerChromiumWithInstrumentedDestructor(&parentDelegate));
    RefPtr<LayerChromium> child = LayerChromium::create(&childDelegate);

    // Set up initial test conditions
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->addChild(child));
    EXPECT_TRUE(child->parent());

    // When being destroyed, notifySyncRequired is called once for the parent, because it has one child,
    // but should not be called for the child which has no children.
    EXPECT_CALL(parentDelegate, notifySyncRequired()).Times(1);
    EXPECT_CALL(childDelegate, notifySyncRequired()).Times(0);

    ASSERT_EQ(0, LayerChromiumWithInstrumentedDestructor::getNumInstancesDestroyed());
    parent.release();
    ASSERT_EQ(1, LayerChromiumWithInstrumentedDestructor::getNumInstancesDestroyed());

    // Child should have been un-parented correctly, but not yet destroyed since we have a reference to it.
    EXPECT_FALSE(child->parent());
}

TEST_F(LayerChromiumTest, verifyDestructorDoesNotLeak)
{
    // In this test we explicitly instantiate a special subclass of
    // LayerChromium so we can track the number of destructors called.

    RefPtr<LayerChromiumWithInstrumentedDestructor> parent, child1, child2, child3, grandChild1, grandChild2, grandChild3;
    parent = adoptRef(new LayerChromiumWithInstrumentedDestructor(&m_silentDelegate));
    child1 = adoptRef(new LayerChromiumWithInstrumentedDestructor(&m_silentDelegate));
    child2 = adoptRef(new LayerChromiumWithInstrumentedDestructor(&m_silentDelegate));
    child3 = adoptRef(new LayerChromiumWithInstrumentedDestructor(&m_silentDelegate));
    grandChild1 = adoptRef(new LayerChromiumWithInstrumentedDestructor(&m_silentDelegate));
    grandChild2 = adoptRef(new LayerChromiumWithInstrumentedDestructor(&m_silentDelegate));
    grandChild3 = adoptRef(new LayerChromiumWithInstrumentedDestructor(&m_silentDelegate));

    // set up a simple tree.
    parent->addChild(child1);
    parent->addChild(child2);
    parent->addChild(child3);
    child1->addChild(grandChild1);
    child1->addChild(grandChild2);
    child2->addChild(grandChild3);

    // Clear all the children RefPtrs here. But since they are attached to the tree, no destructors should be called yet.
    child1.clear();
    child2.clear();
    child3.clear();
    grandChild1.clear();
    grandChild2.clear();
    grandChild3.clear();

    // releasing the parent should cause all destructors to be invoked.
    ASSERT_EQ(0, LayerChromiumWithInstrumentedDestructor::getNumInstancesDestroyed());
    parent.release();
    ASSERT_EQ(7, LayerChromiumWithInstrumentedDestructor::getNumInstancesDestroyed());
}

TEST_F(LayerChromiumTest, insertChild)
{
    MockLayerDelegate parentDelegate;
    MockLayerDelegate childDelegate;
    RefPtr<LayerChromium>parent = LayerChromium::create(&parentDelegate);
    RefPtr<LayerChromium>child1 = LayerChromium::create(&childDelegate);
    RefPtr<LayerChromium>child2 = LayerChromium::create(&childDelegate);
    RefPtr<LayerChromium>child3 = LayerChromium::create(&childDelegate);
    RefPtr<LayerChromium>child4 = LayerChromium::create(&childDelegate);

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    // The child delegate notifySyncRequired should not be called when inserting.
    EXPECT_CALL(childDelegate, notifySyncRequired()).Times(0);

    // Case 1: inserting to empty list.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->insertChild(child3, 0));
    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child3, parent->children()[0]);
    EXPECT_EQ(parent.get(), child3->parent());

    // Case 2: inserting to beginning of list
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->insertChild(child1, 0));
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child3, parent->children()[1]);
    EXPECT_EQ(parent.get(), child1->parent());

    // Case 3: inserting to middle of list
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->insertChild(child2, 1));
    ASSERT_EQ(static_cast<size_t>(3), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(parent.get(), child2->parent());

    // Case 4: inserting to end of list
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->insertChild(child4, 3));

    ASSERT_EQ(static_cast<size_t>(4), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(child4, parent->children()[3]);
    EXPECT_EQ(parent.get(), child4->parent());

    // parent's destructor will invoke notifySyncRequired as it removes its children.
    EXPECT_CALL(parentDelegate, notifySyncRequired()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, insertChildPastEndOfList)
{
    RefPtr<LayerChromium> parent = LayerChromium::create(&m_silentDelegate);
    RefPtr<LayerChromium> child1 = LayerChromium::create(&m_silentDelegate);
    RefPtr<LayerChromium> child2 = LayerChromium::create(&m_silentDelegate);

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
    MockLayerDelegate parentDelegate;
    RefPtr<LayerChromium> parent = LayerChromium::create(&parentDelegate);
    RefPtr<LayerChromium> child1 = LayerChromium::create(&m_silentDelegate);
    RefPtr<LayerChromium> child2 = LayerChromium::create(&m_silentDelegate);

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->insertChild(child1, 0));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, 1, parent->insertChild(child2, 1));

    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);

    // Inserting the same child again should cause the child to be removed and re-inserted at the new location.
    // So the parent's notifySyncRequired would be called one or more times.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(parentDelegate, AtLeast(1), parent->insertChild(child1, 1));

    // child1 should now be at the end of the list.
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child2, parent->children()[0]);
    EXPECT_EQ(child1, parent->children()[1]);

    // parent's destructor will invoke notifySyncRequired as it removes leftover children
    EXPECT_CALL(parentDelegate, notifySyncRequired()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, insertChildThatAlreadyHadParent)
{
    MockLayerDelegate oldParentDelegate;
    RefPtr<LayerChromium> oldParent = LayerChromium::create(&oldParentDelegate);
    RefPtr<LayerChromium> parent = LayerChromium::create(&m_silentDelegate);
    RefPtr<LayerChromium> child = LayerChromium::create(&m_silentDelegate);

    // set up and sanity-check initial test conditions
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(oldParentDelegate, 1, oldParent->addChild(child));
    ASSERT_EQ(static_cast<size_t>(1), oldParent->children().size());
    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());
    EXPECT_EQ(child, oldParent->children()[0]);
    EXPECT_EQ(oldParent.get(), child->parent());

    // Inserting to new parent causes old parent's notifySyncRequired to be called.
    // Note that it also causes parent's notifySyncRequired to be called, but that is tested elsewhere.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(oldParentDelegate, 1, parent->insertChild(child, 0));

    ASSERT_EQ(static_cast<size_t>(0), oldParent->children().size());
    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child, parent->children()[0]);
    EXPECT_EQ(parent.get(), child->parent());
}

TEST_F(LayerChromiumTest, replaceChildWithNewChild)
{
    createSimpleTestTree();
    RefPtr<LayerChromium> child4 = LayerChromium::create(&m_silentDelegate);

    EXPECT_FALSE(child4->parent());

    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(m_parentDelegate, AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    EXPECT_FALSE(m_child2->parent());

    // parent's destructor will invoke notifySyncRequired as it removes leftover children
    EXPECT_CALL(m_parentDelegate, notifySyncRequired()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, replaceChildWithNewChildThatHasOtherParent)
{
    createSimpleTestTree();

    // create another simple tree with testLayer and child4.
    RefPtr<LayerChromium> testLayer = LayerChromium::create(&m_silentDelegate);
    RefPtr<LayerChromium> child4 = LayerChromium::create(&m_silentDelegate);
    testLayer->addChild(child4);
    ASSERT_EQ(static_cast<size_t>(1), testLayer->children().size());
    EXPECT_EQ(child4, testLayer->children()[0]);
    EXPECT_EQ(testLayer.get(), child4->parent());

    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(m_parentDelegate, AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    // testLayer should no longer have child4,
    // and child2 should no longer have a parent.
    ASSERT_EQ(static_cast<size_t>(0), testLayer->children().size());
    EXPECT_FALSE(m_child2->parent());

    // parent's destructor will invoke notifySyncRequired as it removes leftover children
    EXPECT_CALL(m_parentDelegate, notifySyncRequired()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, replaceChildWithSameChild)
{
    createSimpleTestTree();

    // notifySyncRequired should not be called because its the same child
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(m_parentDelegate, 0, m_parent->replaceChild(m_child2.get(), m_child2));

    verifyTestTreeInitialState();

    // parent's destructor will invoke notifySyncRequired as it removes leftover children
    EXPECT_CALL(m_parentDelegate, notifySyncRequired()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, removeAllChildren)
{
    createSimpleTestTree();

    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(m_parentDelegate, AtLeast(1), m_parent->removeAllChildren());

    ASSERT_EQ(static_cast<size_t>(0), m_parent->children().size());
    EXPECT_FALSE(m_child1->parent());
    EXPECT_FALSE(m_child2->parent());
    EXPECT_FALSE(m_child3->parent());

    // notifySyncRequired should not be called if trying to removeAllChildren when there are no children.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(m_parentDelegate, 0, m_parent->removeAllChildren());
}

TEST_F(LayerChromiumTest, setChildren)
{
    MockLayerDelegate newParentDelegate;
    RefPtr<LayerChromium> oldParent = LayerChromium::create(&m_silentDelegate);
    RefPtr<LayerChromium> newParent = LayerChromium::create(&newParentDelegate);

    RefPtr<LayerChromium> child1 = LayerChromium::create(&m_silentDelegate);
    RefPtr<LayerChromium> child2 = LayerChromium::create(&m_silentDelegate);

    Vector<RefPtr<LayerChromium> > newChildren;
    newChildren.append(child1);
    newChildren.append(child2);

    // Set up and verify initial test conditions: child1 has a parent, child2 has no parent.
    oldParent->addChild(child1);
    ASSERT_EQ(static_cast<size_t>(0), newParent->children().size());
    EXPECT_EQ(oldParent.get(), child1->parent());
    EXPECT_FALSE(child2->parent());

    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(newParentDelegate, AtLeast(1), newParent->setChildren(newChildren));

    ASSERT_EQ(static_cast<size_t>(2), newParent->children().size());
    EXPECT_EQ(newParent.get(), child1->parent());
    EXPECT_EQ(newParent.get(), child2->parent());

    // parent's destructor will invoke notifySyncRequired as it removes its children.
    EXPECT_CALL(newParentDelegate, notifySyncRequired()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, getRootLayerAfterTreeManipulations)
{
    createSimpleTestTree();
    RefPtr<LayerChromium> child4 = LayerChromium::create(&m_silentDelegate);

    // In this test case, we don't care about how the parent's notifySyncRequired is called.
    EXPECT_CALL(m_parentDelegate, notifySyncRequired()).Times(AnyNumber());

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
    //   2. indirectly calls notifySyncRequired, exactly once for each call to setNeedsDisplay.

    MockLayerDelegate mockDelegate;
    RefPtr<LayerChromium> testLayer = LayerChromium::create(&mockDelegate);
    IntSize testBounds = IntSize(501, 508);

    FloatRect dirty1 = FloatRect(10.0f, 15.0f, 1.0f, 2.0f);
    FloatRect dirty2 = FloatRect(20.0f, 25.0f, 3.0f, 4.0f);
    FloatRect emptyDirtyRect = FloatRect(40.0f, 45.0f, 0, 0);
    FloatRect outOfBoundsDirtyRect = FloatRect(400.0f, 405.0f, 500.0f, 502.0f);

    // Before anything, testLayer should not be dirty.
    EXPECT_FALSE(testLayer->needsDisplay());

    // This is just initialization, but notifySyncRequired behavior is verified anyway to avoid warnings.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setBounds(testBounds));
    testLayer = LayerChromium::create(&mockDelegate);
    EXPECT_FALSE(testLayer->needsDisplay());

    // The real test begins here.

    // Case 1: needsDisplay flag should not change because of an empty dirty rect.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setNeedsDisplayRect(emptyDirtyRect));
    EXPECT_FALSE(testLayer->needsDisplay());

    // Case 2: basic.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setNeedsDisplayRect(dirty1));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 3: a second dirty rect.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setNeedsDisplayRect(dirty2));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 4: LayerChromium should accept dirty rects that go beyond its bounds.
    testLayer = LayerChromium::create(&mockDelegate);
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setBounds(testBounds));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setNeedsDisplayRect(outOfBoundsDirtyRect));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 5: setNeedsDisplay() without the dirty rect arg.
    testLayer = LayerChromium::create(&mockDelegate);
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setBounds(testBounds));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setNeedsDisplay());
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 6: setNeedsDisplay() without the dirty rect arg should not cause
    // needsDisplay flag to change for LayerChromium with empty bounds.
    testLayer = LayerChromium::create(&mockDelegate);
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setNeedsDisplay());
    EXPECT_FALSE(testLayer->needsDisplay());
}

TEST_F(LayerChromiumTest, checkSetNeedsDisplayWithNullDelegate)
{
    // Without a delegate, the layer should still mark itself dirty as appropriate,
    // and it should not crash trying to use a non-existing delegate.
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
    MockLayerDelegate initialDelegate;
    MockLayerDelegate mockDelegate;
    RefPtr<LayerChromium> testLayer = LayerChromium::create(&initialDelegate);
    RefPtr<LayerChromium> dummyLayer = LayerChromium::create(&m_silentDelegate); // just a dummy layer for this test case.

    // sanity check of initial test condition
    EXPECT_FALSE(testLayer->needsDisplay());

    // Test properties that should not call needsDisplay and needsCommit when changed.
    // notifySyncRequired should not be called, and the needsDisplay flag should remain false.
    EXPECT_CALL(initialDelegate, notifySyncRequired()).Times(0); // old delegate should not be used when setDelegate gives a new delegate.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setDelegate(&mockDelegate));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setName("Test Layer"));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setVisibleLayerRect(IntRect(0, 0, 40, 50)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setUsesLayerClipping(true));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setIsNonCompositedContent(true));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setDrawOpacity(0.5f));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setClipRect(IntRect(3, 3, 8, 8)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setTargetRenderSurface(0));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setDrawTransform(TransformationMatrix()));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setScreenSpaceTransform(TransformationMatrix()));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 0, testLayer->setDrawableContentRect(IntRect(4, 5, 6, 7)));
    EXPECT_FALSE(testLayer->needsDisplay());

    // Next, test properties that should call setNeedsCommit (but not setNeedsDisplay)
    // These properties should indirectly call notifySyncRequired, but the needsDisplay flag should not change.
    // All properties need to be set to new values in order for setNeedsCommit
    // to be called.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setAnchorPoint(FloatPoint(1.23f, 4.56f)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setAnchorPointZ(0.7f));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setBackgroundColor(Color(0.4f, 0.4f, 0.4f)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setMasksToBounds(true));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setMaskLayer(dummyLayer.get()));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setOpacity(0.5f));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setOpaque(true));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setPosition(FloatPoint(4.0f, 9.0f)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setReplicaLayer(dummyLayer.get()));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setSublayerTransform(TransformationMatrix(0, 0, 0, 0, 0, 0)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setScrollable(true));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setScrollPosition(IntPoint(10, 10)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setTransform(TransformationMatrix(0, 0, 0, 0, 0, 0)));
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setDoubleSided(false));

    // The above tests should not have caused a change to the needsDisplay flag.
    EXPECT_FALSE(testLayer->needsDisplay());

    // Test properties that should call setNeedsDisplay
    // These properties will call notifySyncRequired and change the needsDisplay flag.
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setBounds(IntSize(5, 10)));
    EXPECT_TRUE(testLayer->needsDisplay());

    // FIXME: need to add a test for setLayerTreeHost with a non-null stubbed CCLayerTreeHost.
}

TEST_F(LayerChromiumTest, checkNonCompositedContentPropertyChangeCausesCommit)
{
    OwnPtr<MockNonCompositedContentHost> nonCompositedContentHost(MockNonCompositedContentHost::create());

    GraphicsLayer* rootLayer = nonCompositedContentHost->topLevelRootLayer();

    EXPECT_CALL(*nonCompositedContentHost, notifySyncRequired(_)).Times(1);
    rootLayer->platformLayer()->setScrollPosition(IntPoint(1, 1));
    Mock::VerifyAndClearExpectations(nonCompositedContentHost.get());

    EXPECT_CALL(*nonCompositedContentHost, notifySyncRequired(_)).Times(AtLeast(1));
    nonCompositedContentHost->setViewport(IntSize(30, 30), IntSize(20, 20), IntPoint(10, 10), 1);
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
    MockLayerDelegate mockDelegate;
    RefPtr<LayerChromiumWithContentScaling> testLayer = adoptRef(new LayerChromiumWithContentScaling(&mockDelegate));

    IntSize testBounds = IntSize(320, 240);
    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setBounds(testBounds));

    testLayer->resetNeedsDisplay();
    EXPECT_FALSE(testLayer->needsDisplay());

    EXECUTE_AND_VERIFY_NOTIFY_SYNC_BEHAVIOR(mockDelegate, 1, testLayer->setContentsScale(testLayer->contentsScale() + 1.f));
    EXPECT_TRUE(testLayer->needsDisplay());
}

} // namespace
