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

#include "config.h"

#include <string>
#include <wtf/CrossThreadTask.h>

namespace TestWebKitAPI {

struct LifetimeLogger {
    LifetimeLogger()
    {
        log() << "default_constructor(" << &name << "-" << copyGeneration << "-" << moveGeneration << ") ";
    }

    LifetimeLogger(const char* inputName)
        : name(*inputName)
    {
        log() << "name_constructor(" << &name << "-" << copyGeneration << "-" << moveGeneration << ") ";
    }

    LifetimeLogger(const LifetimeLogger& other)
        : name(other.name)
        , copyGeneration(other.copyGeneration + 1)
        , moveGeneration(other.moveGeneration)
    {
        log() << "copy_constructor(" << &name << "-" << copyGeneration << "-" << moveGeneration << ") ";
    }

    LifetimeLogger(LifetimeLogger&& other)
        : name(other.name)
        , copyGeneration(other.copyGeneration)
        , moveGeneration(other.moveGeneration + 1)
    {
        log() << "move_constructor(" << &name << "-" << copyGeneration << "-" << moveGeneration << ") ";
    }

    ~LifetimeLogger()
    {
        log() << "destructor(" << &name << "-" << copyGeneration << "-" << moveGeneration << ") ";
    }

    LifetimeLogger isolatedCopy() const
    {
        log() << "isolatedCopy() ";
        return LifetimeLogger(*this);
    }

    const char& name { *"<default>" };
    int copyGeneration { 0 };
    int moveGeneration { 0 };

    static std::ostringstream& log()
    {
        static std::ostringstream log;
        return log;
    }

    static std::string takeLogStr()
    {
        std::string string = log().str();
        log().str("");
        return string;
    }
};

void testFunction(const LifetimeLogger&, const LifetimeLogger&, const LifetimeLogger&)
{
    LifetimeLogger::log() << "testFunction called" << " ";
}

TEST(WTF_CrossThreadTask, Basic)
{
    {
        LifetimeLogger logger1;
        LifetimeLogger logger2(logger1);
        LifetimeLogger logger3("logger");

        auto task = createCrossThreadTask(testFunction, logger1, logger2, logger3);
        task.performTask();
    }
    ASSERT_STREQ("default_constructor(<default>-0-0) copy_constructor(<default>-1-0) name_constructor(logger-0-0) isolatedCopy() copy_constructor(<default>-1-0) isolatedCopy() copy_constructor(<default>-2-0) isolatedCopy() copy_constructor(logger-1-0) move_constructor(<default>-1-1) move_constructor(<default>-2-1) move_constructor(logger-1-1) destructor(logger-1-0) destructor(<default>-2-0) destructor(<default>-1-0) move_constructor(<default>-1-2) move_constructor(<default>-2-2) move_constructor(logger-1-2) destructor(logger-1-1) destructor(<default>-2-1) destructor(<default>-1-1) testFunction called destructor(logger-1-2) destructor(<default>-2-2) destructor(<default>-1-2) destructor(logger-0-0) destructor(<default>-1-0) destructor(<default>-0-0) ", LifetimeLogger::takeLogStr().c_str());
}
    
} // namespace TestWebKitAPI
