/*
* Copyright (C) 2009 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef InspectorTimelineAgent_h
#define InspectorTimelineAgent_h

#include "Document.h"
#include "ScriptExecutionContext.h"
#include "ScriptObject.h"
#include "ScriptArray.h"
#include <wtf/Vector.h>

namespace WebCore {
    class Event;
    class InspectorFrontend;

    // Must be kept in sync with TimelineAgent.js
    enum TimelineRecordType {
        EventDispatchTimelineRecordType = 0,
        LayoutTimelineRecordType = 1,
        RecalculateStylesTimelineRecordType = 2,
        PaintTimelineRecordType = 3,
        ParseHTMLTimelineRecordType = 4,
        TimerInstallTimelineRecordType = 5,
        TimerRemoveTimelineRecordType = 6,
        TimerFireTimelineRecordType = 7,
        XHRReadyStateChangeRecordType = 8,
        XHRLoadRecordType = 9,
        EvaluateScriptTimelineRecordType = 10,
        MarkTimelineRecordType = 11,
    };

    class InspectorTimelineAgent {
    public:
        InspectorTimelineAgent(InspectorFrontend* frontend);
        ~InspectorTimelineAgent();

        void reset();
        void resetFrontendProxyObject(InspectorFrontend*);

        // Methods called from WebCore.
        void willDispatchEvent(const Event&);
        void didDispatchEvent();

        void willLayout();
        void didLayout();

        void willRecalculateStyle();
        void didRecalculateStyle();

        void willPaint();
        void didPaint();

        void willWriteHTML();
        void didWriteHTML();
        
        void didInstallTimer(int timerId, int timeout, bool singleShot);
        void didRemoveTimer(int timerId);
        void willFireTimer(int timerId);
        void didFireTimer();

        void willChangeXHRReadyState(const String&, int);
        void didChangeXHRReadyState();
        void willLoadXHR(const String&);
        void didLoadXHR();

        void willEvaluateScript(const String&, int);
        void didEvaluateScript();

        void didMarkTimeline(const String&);

        static InspectorTimelineAgent* retrieve(ScriptExecutionContext*);
    private:
        struct TimelineRecordEntry {
            TimelineRecordEntry(ScriptObject record, ScriptArray children, TimelineRecordType type) : record(record), children(children), type(type) { }
            ScriptObject record;
            ScriptArray children;
            TimelineRecordType type;
        };
        
        void pushCurrentRecord(ScriptObject, TimelineRecordType);
        
        static double currentTimeInMilliseconds();

        void didCompleteCurrentRecord(TimelineRecordType);
        
        void addRecordToTimeline(ScriptObject, TimelineRecordType);

        InspectorFrontend* m_frontend;
        
        Vector< TimelineRecordEntry > m_recordStack;
    };

inline InspectorTimelineAgent* InspectorTimelineAgent::retrieve(ScriptExecutionContext* context)
{
    if (context->isDocument())
        return static_cast<Document*>(context)->inspectorTimelineAgent();
    return 0;
}

} // namespace WebCore

#endif // !defined(InspectorTimelineAgent_h)
