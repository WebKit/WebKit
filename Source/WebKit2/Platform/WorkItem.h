/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WorkItem_h
#define WorkItem_h

#include <wtf/Functional.h>
#include <wtf/PassOwnPtr.h>

class WorkItem {
public:
    template<typename C> 
    static PassOwnPtr<WorkItem> create(C*, void (C::*)());

    template<typename C, typename T0>
    static PassOwnPtr<WorkItem> create(C*, void (C::*)(T0), T0);

    template<typename C, typename T0, typename T1>
    static PassOwnPtr<WorkItem> create(C*, void (C::*)(T0, T1), T0, T1);

    static PassOwnPtr<WorkItem> create(void (*)());

    template<typename C>
    static PassOwnPtr<WorkItem> createDeref(C*);

    static PassOwnPtr<WorkItem> create(const Function<void()>&);

    virtual ~WorkItem() { }
    virtual void execute() = 0;

protected:
    WorkItem() { }

private:
    WorkItem(const WorkItem&);
    WorkItem& operator=(const WorkItem&);
};

template <typename C>
class MemberFunctionWorkItem0 : private WorkItem {
    // We only allow WorkItem to create this.
    friend class WorkItem;

    typedef void (C::*FunctionType)();
    
    MemberFunctionWorkItem0(C* ptr, FunctionType function)
        : m_ptr(ptr)
        , m_function(function)
    {
        m_ptr->ref();
    }

    ~MemberFunctionWorkItem0()
    {
        m_ptr->deref();
    }

    virtual void execute()
    {
        (m_ptr->*m_function)();
    }

    C* m_ptr;
    FunctionType m_function;
};

template<typename C, typename T0>
class MemberFunctionWorkItem1 : private WorkItem {
    // We only allow WorkItem to create this.
    friend class WorkItem;
    
    typedef void (C::*FunctionType)(T0);
    
    MemberFunctionWorkItem1(C* ptr, FunctionType function, T0 t0)
        : m_ptr(ptr)
        , m_function(function)
        , m_t0(t0)
    {
        m_ptr->ref();
    }
    
    ~MemberFunctionWorkItem1()
    {
        m_ptr->deref();
    }

    virtual void execute()
    {
        (m_ptr->*m_function)(m_t0);
    }
    
    C* m_ptr;
    FunctionType m_function;
    T0 m_t0;
};

template<typename C, typename T0, typename T1>
class MemberFunctionWorkItem2 : private WorkItem {
    // We only allow WorkItem to create this.
    friend class WorkItem;
    
    typedef void (C::*FunctionType)(T0, T1);
    
    MemberFunctionWorkItem2(C* ptr, FunctionType function, T0 t0, T1 t1)
        : m_ptr(ptr)
        , m_function(function)
        , m_t0(t0)
        , m_t1(t1)
    {
        m_ptr->ref();
    }
    
    ~MemberFunctionWorkItem2()
    {
        m_ptr->deref();
    }

    virtual void execute()
    {
        (m_ptr->*m_function)(m_t0, m_t1);
    }
    
    C* m_ptr;
    FunctionType m_function;
    T0 m_t0;
    T1 m_t1;
};

template<typename C>
PassOwnPtr<WorkItem> WorkItem::create(C* ptr, void (C::*function)())
{
    return adoptPtr(static_cast<WorkItem*>(new MemberFunctionWorkItem0<C>(ptr, function)));
}

template<typename C, typename T0>
PassOwnPtr<WorkItem> WorkItem::create(C* ptr, void (C::*function)(T0), T0 t0)
{
    return adoptPtr(static_cast<WorkItem*>(new MemberFunctionWorkItem1<C, T0>(ptr, function, t0)));
}

template<typename C, typename T0, typename T1>
PassOwnPtr<WorkItem> WorkItem::create(C* ptr, void (C::*function)(T0, T1), T0 t0, T1 t1)
{
    return adoptPtr(static_cast<WorkItem*>(new MemberFunctionWorkItem2<C, T0, T1>(ptr, function, t0, t1)));
}

class FunctionWorkItem0 : private WorkItem {
    // We only allow WorkItem to create this.
    friend class WorkItem;

    typedef void (*FunctionType)();
    
    FunctionWorkItem0(FunctionType function)
        : m_function(function)
    {
    }

    virtual void execute()
    {
        (*m_function)();
    }

    FunctionType m_function;
};

inline PassOwnPtr<WorkItem> WorkItem::create(void (*function)())
{
    return adoptPtr(static_cast<WorkItem*>(new FunctionWorkItem0(function)));
}

template<typename C>
class DerefWorkItem : private WorkItem {
    // We only allow WorkItem to create this.
    friend class WorkItem;

    explicit DerefWorkItem(C* ptr)
        : m_ptr(ptr)
    {
    }

    virtual void execute()
    {
        m_ptr->deref();
    }

    C* m_ptr;
};

template<typename C>
PassOwnPtr<WorkItem> WorkItem::createDeref(C* ptr)
{
    return adoptPtr(static_cast<WorkItem*>(new DerefWorkItem<C>(ptr)));
}

// FunctionWorkItem is a temporary class. WorkItem should just be replaced by WTF::Function everywhere.
class FunctionWorkItem : private WorkItem {
    // We only allow WorkItem to create this.
    friend class WorkItem;

    explicit FunctionWorkItem(const Function<void()>& function)
        : m_function(function)
    {
    }

    virtual void execute()
    {
        m_function();
    }

    Function<void()> m_function;
};

inline PassOwnPtr<WorkItem> WorkItem::create(const Function<void()>& function)
{
    return adoptPtr(static_cast<WorkItem*>(new FunctionWorkItem(function)));
}

#endif // WorkItem_h
