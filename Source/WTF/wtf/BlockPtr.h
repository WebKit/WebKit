/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef BlockPtr_h
#define BlockPtr_h

#include <Block.h>
#include <wtf/Assertions.h>

namespace WTF {

template<typename> class BlockPtr;

template<typename R, typename... Args>
class BlockPtr<R (Args...)> {
public:
    BlockPtr()
        : m_block(nullptr)
    {
    }

    BlockPtr(R (^block)(Args...))
        : m_block(Block_copy(block))
    {
    }

    BlockPtr(const BlockPtr& other)
        : m_block(Block_copy(other.m_block))
    {
    }
    
    BlockPtr(BlockPtr&& other)
        : m_block(std::exchange(other.m_block, nullptr))
    {
    }
    
    ~BlockPtr()
    {
        Block_release(m_block);
    }

    BlockPtr& operator=(const BlockPtr& other)
    {
        if (this != &other) {
            Block_release(m_block);
            m_block = Block_copy(other.m_block);
        }
        
        return *this;
    }

    BlockPtr& operator=(BlockPtr&& other)
    {
        ASSERT(this != &other);

        Block_release(m_block);
        m_block = std::exchange(other.m_block, nullptr);

        return *this;
    }

    explicit operator bool() const { return m_block; }
    bool operator!() const { return !m_block; }

    R operator()(Args... arguments) const
    {
        ASSERT(m_block);
        
        return m_block(std::forward<Args>(arguments)...);
    }
    
private:
    R (^m_block)(Args...);
};

template<typename R, typename... Args>
inline BlockPtr<R (Args...)> makeBlockPtr(R (^block)(Args...))
{
    return BlockPtr<R (Args...)>(block);
}

}

using WTF::BlockPtr;
using WTF::makeBlockPtr;

#endif // BlockPtr_h
