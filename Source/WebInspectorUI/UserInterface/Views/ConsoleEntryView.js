/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

WI.ConsoleEntryView = class ConsoleEntryView extends WI.Object
{
    constructor()
    {
        super();
    }
    
    // Public

    setMessageLevel(messageLevel)
    {
        console.assert(this._element);
    
        switch (messageLevel) {
        case WI.ConsoleMessage.MessageLevel.Log:
            this._element.classList.add("console-log-level");
            this._element.setAttribute("data-labelprefix", WI.UIString("Log: "));
            break;
        case WI.ConsoleMessage.MessageLevel.Info:
            this._element.classList.add("console-info-level");
            this._element.setAttribute("data-labelprefix", WI.UIString("Info: "));
            break;
        case WI.ConsoleMessage.MessageLevel.Debug:
            this._element.classList.add("console-debug-level");
            this._element.setAttribute("data-labelprefix", WI.UIString("Debug: "));
            break;
        case WI.ConsoleMessage.MessageLevel.Warning:
            this._element.classList.add("console-warning-level");
            this._element.setAttribute("data-labelprefix", WI.UIString("Warning: "));
            break;
        case WI.ConsoleMessage.MessageLevel.Error:
            this._element.classList.add("console-error-level");
            this._element.setAttribute("data-labelprefix", WI.UIString("Error: "));
            break;
        }
    }
};
