/*
 * Copyright 2008-2010 Stephan Aßmus <superstippi@gmx.de>
 * All rights reserved. Distributed under the terms of the MIT/X11 license.
 */
#ifndef FunctionTracer_h
#define FunctionTracer_h

#include <stdio.h>

class FunctionTracer;

static FunctionTracer* sPreviousFunctionTracer;

#ifndef CLASS_NAME
#define CLASS_NAME "CLASS_NAME"
#endif

class FunctionTracer {
public:
    FunctionTracer(const void* ptr, const char* functionName, int& depth)
	    : m_previousTracer(sPreviousFunctionTracer)
	    , m_blockWasNeeded(false)
	    , m_functionDepth(depth)
	    , m_thisPointer(ptr)
	    , m_functionName(functionName)
    {
        m_functionDepth++;
        if (m_previousTracer)
            m_previousTracer->blockNeeded();
        sPreviousFunctionTracer = this;
	}

	~FunctionTracer()
	{
		if (m_blockWasNeeded)
            printf("%*s}\n", m_functionDepth * 2, "");
        else
            printf("\n");
        m_functionDepth--;
        sPreviousFunctionTracer = m_previousTracer;
	}

    void init(const char* format = "", ...)
    {
        char buffer[1024];
        va_list list;
        va_start(list, format);
        vsnprintf(buffer, sizeof(buffer), format, list);
        va_end(list);
        printf("%*s%p->%s::%s(%s)", m_functionDepth * 2, "", m_thisPointer,
               CLASS_NAME, m_functionName, buffer);
    }

	void blockNeeded()
	{
        if (!m_blockWasNeeded) {
	        printf(" {\n");
            m_blockWasNeeded = true;
        }
	}

private:
    FunctionTracer* m_previousTracer;
    bool m_blockWasNeeded;
    int& m_functionDepth;
    const void* m_thisPointer;
    const char* m_functionName;
};

static int sFunctionDepth = -1;

#define CALLED(x...)    FunctionTracer _ft(this, __FUNCTION__, sFunctionDepth); \
                        _ft.init(x)

#define TRACE(x...)     { \
                            if (sPreviousFunctionTracer) \
                                sPreviousFunctionTracer->blockNeeded(); \
                            printf("%*s", (sFunctionDepth + 1) * 2, ""); \
                            printf(x); \
                        }


#endif // FunctionTracer_h
