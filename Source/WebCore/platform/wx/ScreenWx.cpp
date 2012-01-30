/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include "Screen.h"
#include "IntRect.h"
#include "FloatRect.h"
#include "FrameView.h"
#include "NotImplemented.h"
#include "Widget.h"

#include <wx/defs.h>
#include <wx/display.h>
#include <wx/gdicmn.h>

namespace WebCore {
    
int screenHorizontalDPI(Widget*)
{
    notImplemented();
    return 0;
}
int screenVerticalDPI(Widget*)
{
    notImplemented();
    return 0;
}
    
FloatRect screenRect(FrameView* frameView)
{
/*
    int displayNum;
    Widget* widget = widget->widget();
    displayNum = wxDisplay::GetFromWindow(widget->nativeWindow());
    if (displayNum != -1){
        wxDisplay thisDisplay(displayNum);
        return thisDisplay.GetGeometry();
    }
*/
    return FloatRect();
}

int screenDepth(Widget* widget)
{
    return wxDisplayDepth();
}

int screenDepthPerComponent(Widget*)
{
    return wxDisplayDepth();
}

bool screenIsMonochrome(Widget* widget)
{
    return wxColourDisplay();
}

FloatRect screenAvailableRect(FrameView* frameView)
{
/*
    Widget* widget = widget->widget();
    displayNum = wxDisplay::GetFromWindow(widget->nativeWindow());
    if (displayNum != -1){
        wxDisplay thisDisplay(displayNum);
        // FIXME: In 2.7 this method is gone? 
        //return thisDisplay.GetClientArea();
    }
*/    
    return FloatRect();
}

float scaleFactor(Widget*)
{
    return 1.0f;

}

}
