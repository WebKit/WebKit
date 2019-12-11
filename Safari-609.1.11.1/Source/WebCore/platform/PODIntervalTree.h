/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PODIntervalTree_h
#define PODIntervalTree_h

#include "PODInterval.h"
#include "PODRedBlackTree.h"
#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/ValueToString.h>

namespace WebCore {

template <class T, class UserData = void*>
class PODIntervalSearchAdapter {
public:
    typedef PODInterval<T, UserData> IntervalType;
    
    PODIntervalSearchAdapter(Vector<IntervalType>& result, const T& lowValue, const T& highValue)
        : m_result(result)
        , m_lowValue(lowValue)
        , m_highValue(highValue)
    {
    }
    
    const T& lowValue() const { return m_lowValue; }
    const T& highValue() const { return m_highValue; }
    void collectIfNeeded(const IntervalType& data) const
    {
        if (data.overlaps(m_lowValue, m_highValue))
            m_result.append(data);
    }

private:
    Vector<IntervalType>& m_result;
    T m_lowValue;
    T m_highValue;
};

// An interval tree, which is a form of augmented red-black tree. It
// supports efficient (O(lg n)) insertion, removal and querying of
// intervals in the tree.
template<class T, class UserData = void*>
class PODIntervalTree : public PODRedBlackTree<PODInterval<T, UserData>> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(PODIntervalTree);
public:
    // Typedef to reduce typing when declaring intervals to be stored in
    // this tree.
    typedef PODInterval<T, UserData> IntervalType;
    typedef PODIntervalSearchAdapter<T, UserData> IntervalSearchAdapterType;

    PODIntervalTree()
        : PODRedBlackTree<IntervalType>()
    {
        init();
    }

    // Returns all intervals in the tree which overlap the given query
    // interval. The returned intervals are sorted by increasing low
    // endpoint.
    Vector<IntervalType> allOverlaps(const IntervalType& interval) const
    {
        Vector<IntervalType> result;
        allOverlaps(interval, result);
        return result;
    }

    // Returns all intervals in the tree which overlap the given query
    // interval. The returned intervals are sorted by increasing low
    // endpoint.
    void allOverlaps(const IntervalType& interval, Vector<IntervalType>& result) const
    {
        // Explicit dereference of "this" required because of
        // inheritance rules in template classes.
        IntervalSearchAdapterType adapter(result, interval.low(), interval.high());
        searchForOverlapsFrom<IntervalSearchAdapterType>(this->root(), adapter);
    }
    
    template <class AdapterType>
    void allOverlapsWithAdapter(AdapterType& adapter) const
    {
        // Explicit dereference of "this" required because of
        // inheritance rules in template classes.
        searchForOverlapsFrom<AdapterType>(this->root(), adapter);
    }

    // Helper to create interval objects.
    static IntervalType createInterval(const T& low, const T& high, const UserData data = 0)
    {
        return IntervalType(low, high, data);
    }

    Optional<IntervalType> nextIntervalAfter(const IntervalType& interval)
    {
        auto next = smallestNodeGreaterThanFrom(interval, this->root());
        if (!next)
            return WTF::nullopt;

        return next->data();
    }

    bool checkInvariants() const override
    {
        if (!PODRedBlackTree<IntervalType>::checkInvariants())
            return false;
        if (!this->root())
            return true;
        return checkInvariantsFromNode(this->root(), 0);
    }

private:
    typedef typename PODRedBlackTree<IntervalType>::Node IntervalNode;

    // Initializes the tree.
    void init()
    {
        // Explicit dereference of "this" required because of
        // inheritance rules in template classes.
        this->setNeedsFullOrderingComparisons(true);
    }

    // Starting from the given node, adds all overlaps with the given
    // interval to the result vector. The intervals are sorted by
    // increasing low endpoint.
    template <class AdapterType>
    void searchForOverlapsFrom(IntervalNode* node, AdapterType& adapter) const
    {
        if (!node)
            return;

        // Because the intervals are sorted by left endpoint, inorder
        // traversal produces results sorted as desired.

        // See whether we need to traverse the left subtree.
        IntervalNode* left = node->left();
        if (left
            // This is phrased this way to avoid the need for operator
            // <= on type T.
            && !(left->data().maxHigh() < adapter.lowValue()))
            searchForOverlapsFrom<AdapterType>(left, adapter);

        // Check for overlap with current node.
        adapter.collectIfNeeded(node->data());

        // See whether we need to traverse the right subtree.
        // This is phrased this way to avoid the need for operator <=
        // on type T.
        if (!(adapter.highValue() < node->data().low()))
            searchForOverlapsFrom<AdapterType>(node->right(), adapter);
    }

    IntervalNode* smallestNodeGreaterThanFrom(const IntervalType& interval, IntervalNode* node) const
    {
        if (!node)
            return nullptr;

        if (!(interval.high() < node->data().low()))
            return smallestNodeGreaterThanFrom(interval, node->right());

        if (auto left = smallestNodeGreaterThanFrom(interval, node->right()))
            return left;

        return node;
}

    bool updateNode(IntervalNode* node) override
    {
        // Would use const T&, but need to reassign this reference in this
        // function.
        const T* curMax = &node->data().high();
        IntervalNode* left = node->left();
        if (left) {
            if (*curMax < left->data().maxHigh())
                curMax = &left->data().maxHigh();
        }
        IntervalNode* right = node->right();
        if (right) {
            if (*curMax < right->data().maxHigh())
                curMax = &right->data().maxHigh();
        }
        // This is phrased like this to avoid needing operator!= on type T.
        if (!(*curMax == node->data().maxHigh())) {
            node->data().setMaxHigh(*curMax);
            return true;
        }
        return false;
    }

    bool checkInvariantsFromNode(IntervalNode* node, T* currentMaxValue) const
    {
        // These assignments are only done in order to avoid requiring
        // a default constructor on type T.
        T leftMaxValue(node->data().maxHigh());
        T rightMaxValue(node->data().maxHigh());
        IntervalNode* left = node->left();
        IntervalNode* right = node->right();
        if (left) {
            if (!checkInvariantsFromNode(left, &leftMaxValue))
                return false;
        }
        if (right) {
            if (!checkInvariantsFromNode(right, &rightMaxValue))
                return false;
        }
        if (!left && !right) {
            // Base case.
            if (currentMaxValue)
                *currentMaxValue = node->data().high();
            return (node->data().high() == node->data().maxHigh());
        }
        T localMaxValue(node->data().maxHigh());
        if (!left || !right) {
            if (left)
                localMaxValue = leftMaxValue;
            else
                localMaxValue = rightMaxValue;
        } else
            localMaxValue = (leftMaxValue < rightMaxValue) ? rightMaxValue : leftMaxValue;
        if (localMaxValue < node->data().high())
            localMaxValue = node->data().high();
        if (!(localMaxValue == node->data().maxHigh())) {
#ifndef NDEBUG
            String localMaxValueString = ValueToString<T>::string(localMaxValue);
            LOG_ERROR("PODIntervalTree verification failed at node 0x%p: localMaxValue=%s and data=%s",
                      node, localMaxValueString.utf8().data(), node->data().toString().utf8().data());
#endif
            return false;
        }
        if (currentMaxValue)
            *currentMaxValue = localMaxValue;
        return true;
    }
};

} // namespace WebCore

#ifndef NDEBUG
namespace WTF {

// Support for printing PODIntervals at the PODRedBlackTree level.
template<class T, class UserData>
struct ValueToString<WebCore::PODInterval<T, UserData>> {
    static String string(const WebCore::PODInterval<T, UserData>& interval)
    {
        return interval.toString();
    }
};

} // namespace WTF
#endif

#endif // PODIntervalTree_h
