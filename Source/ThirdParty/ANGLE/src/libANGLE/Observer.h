//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Observer:
//   Implements the Observer pattern for sending state change notifications
//   from Subject objects to dependent Observer objects.
//
//   See design document:
//   https://docs.google.com/document/d/15Edfotqg6_l1skTEL8ADQudF_oIdNa7i8Po43k6jMd4/

#ifndef LIBANGLE_OBSERVER_H_
#define LIBANGLE_OBSERVER_H_

#include "common/FastVector.h"
#include "common/angleutils.h"

namespace gl
{
class Context;
}  // namespace gl

namespace angle
{
template <typename HaystackT, typename NeedleT>
bool IsInContainer(const HaystackT &haystack, const NeedleT &needle)
{
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

using SubjectIndex = size_t;

enum class SubjectMessage
{
    CONTENTS_CHANGED,
    STORAGE_CHANGED,
    BINDING_CHANGED,
    DEPENDENT_DIRTY_BITS,
    RESOURCE_MAPPED,
    RESOURCE_UNMAPPED,
};

// The observing class inherits from this interface class.
class ObserverInterface
{
  public:
    virtual ~ObserverInterface();
    virtual void onSubjectStateChange(const gl::Context *context,
                                      SubjectIndex index,
                                      SubjectMessage message) = 0;
};

class ObserverBindingBase
{
  public:
    ObserverBindingBase(ObserverInterface *observer, SubjectIndex subjectIndex)
        : mObserver(observer), mIndex(subjectIndex)
    {}
    virtual ~ObserverBindingBase() {}

    ObserverInterface *getObserver() const { return mObserver; }
    SubjectIndex getSubjectIndex() const { return mIndex; }

    virtual void onSubjectReset() {}

  private:
    ObserverInterface *mObserver;
    SubjectIndex mIndex;
};

// Maintains a list of observer bindings. Sends update messages to the observer.
class Subject : NonCopyable
{
  public:
    Subject();
    virtual ~Subject();

    void onStateChange(const gl::Context *context, SubjectMessage message) const;
    bool hasObservers() const;
    void resetObservers();

    ANGLE_INLINE void addObserver(ObserverBindingBase *observer)
    {
        ASSERT(!IsInContainer(mObservers, observer));
        mObservers.push_back(observer);
    }

    ANGLE_INLINE void removeObserver(ObserverBindingBase *observer)
    {
        ASSERT(IsInContainer(mObservers, observer));
        mObservers.remove_and_permute(observer);
    }

  private:
    // Keep a short list of observers so we can allocate/free them quickly. But since we support
    // unlimited bindings, have a spill-over list of that uses dynamic allocation.
    static constexpr size_t kMaxFixedObservers = 8;
    angle::FastVector<ObserverBindingBase *, kMaxFixedObservers> mObservers;
};

// Keeps a binding between a Subject and Observer, with a specific subject index.
class ObserverBinding final : public ObserverBindingBase
{
  public:
    ObserverBinding(ObserverInterface *observer, SubjectIndex index);
    ~ObserverBinding() override;
    ObserverBinding(const ObserverBinding &other);
    ObserverBinding &operator=(const ObserverBinding &other);

    void bind(Subject *subject);

    ANGLE_INLINE void reset() { bind(nullptr); }

    void onStateChange(const gl::Context *context, SubjectMessage message) const;
    void onSubjectReset() override;

    ANGLE_INLINE const Subject *getSubject() const { return mSubject; }

    ANGLE_INLINE void assignSubject(Subject *subject) { mSubject = subject; }

  private:
    Subject *mSubject;
};

}  // namespace angle

#endif  // LIBANGLE_OBSERVER_H_
