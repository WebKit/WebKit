/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#ifndef ProfileNode_h
#define ProfileNode_h

#include <kjs/ustring.h>
#include <wtf/Deque.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/StrHash.h>

namespace KJS {

    class ProfileNode;

    typedef Deque<RefPtr<ProfileNode> >::const_iterator StackIterator;
    typedef HashCountedSet<UString::Rep*> FunctionCallHashCount;

    class ProfileNode : public RefCounted<ProfileNode> {
    public:
        static PassRefPtr<ProfileNode> create(const UString& name) { return adoptRef(new ProfileNode(name)); }

        void willExecute();
        void didExecute(Vector<UString> stackNames, unsigned int stackIndex);

        void addChild(PassRefPtr<ProfileNode> prpChild);
        ProfileNode* findChild(const UString& name);

        void stopProfiling();

        UString functionName() const { return m_functionName; }
        double totalTime() const { return m_timeSum; }
        double selfTime() const;
        double totalPercent() const;
        double selfPercent() const;
        unsigned numberOfCalls() const { return m_numberOfCalls; }
        const Deque<RefPtr<ProfileNode> >& children() { return m_children; }

        void printDataInspectorStyle(int indentLevel) const;
        double printDataSampleStyle(int indentLevel, FunctionCallHashCount&) const;

    private:
        ProfileNode(const UString& name);

        void endAndRecordCall();
    
        UString m_functionName;
        double m_timeSum;
        double m_startTime;
        unsigned m_numberOfCalls;

        Deque<RefPtr<ProfileNode> > m_children;
    };

} // namespace KJS

#endif // ProfileNode_h
