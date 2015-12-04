/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

WebInspector.ConsoleTabContentView = class ConsoleTabContentView extends WebInspector.ContentBrowserTabContentView
{
    constructor(identifier)
    {
        let {image, title} = WebInspector.ConsoleTabContentView.tabInfo();
        let tabBarItem = new WebInspector.TabBarItem(image, title);

        super(identifier || "console", "console", tabBarItem, null, null, true);
    }

    static tabInfo()
    {
        return {
            image: "Images/Console.svg",
            title: WebInspector.UIString("Console"),
        };
    }

    // Public

    get type()
    {
        return WebInspector.ConsoleTabContentView.Type;
    }

    shown()
    {
        super.shown();

        WebInspector.consoleContentView.prompt.focus();

        if (this.contentBrowser.currentContentView === WebInspector.consoleContentView)
            return;

        // Be sure to close the view in the split content browser before showing it in the
        // tab content browser. We can only show a content view in one browser at a time.
        if (WebInspector.consoleContentView.parentContainer)
            WebInspector.consoleContentView.parentContainer.closeContentView(WebInspector.consoleContentView);

        this.contentBrowser.showContentView(WebInspector.consoleContentView);

        console.assert(this.contentBrowser.currentContentView === WebInspector.consoleContentView);
    }

    showRepresentedObject(representedObject, cookie)
    {
        // Do nothing. The shown function will handle things. Calling
        // ContentBrowserTabContentView.shown will cause a new LogContentView
        // to be created instead of reusing WebInspector.consoleContentView.
        console.assert(representedObject instanceof WebInspector.LogObject);
    }

    canShowRepresentedObject(representedObject)
    {
        return representedObject instanceof WebInspector.LogObject;
    }

    get supportsSplitContentBrowser()
    {
        return false;
    }
};

WebInspector.ConsoleTabContentView.Type = "console";
