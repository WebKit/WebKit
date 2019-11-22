/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

WI.TabActivityDiagnosticEventRecorder = class TabActivityDiagnosticEventRecorder extends WI.DiagnosticEventRecorder
{
    constructor(controller)
    {
        super("TabActivity", controller);

        this._shouldLogTabContentActivity = false;
        this._tabActivityTimeoutIdentifier = 0;
    }

    // Protected

    setup()
    {
        WI.TabBrowser.addEventListener(WI.TabBrowser.Event.SelectedTabContentViewDidChange, this._handleTabBrowserSelectedTabContentViewDidChange, this);

        const options = {
            capture: true,
        };
        window.addEventListener("focus", this, options);
        window.addEventListener("blur", this, options);
        window.addEventListener("keydown", this, options);
        window.addEventListener("mousedown", this, options);

        this._shouldLogTabContentActivity = true;
    }

    teardown()
    {
        WI.TabBrowser.removeEventListener(WI.TabBrowser.Event.SelectedTabContentViewDidChange, this._handleTabBrowserSelectedTabContentViewDidChange, this);

        const options = {
            capture: true,
        };
        window.removeEventListener("focus", this, options);
        window.removeEventListener("blur", this, options);
        window.removeEventListener("keydown", this, options);
        window.removeEventListener("mousedown", this, options);

        this._stopTrackingTabActivity();
    }

    // Public

    handleEvent(event)
    {
        switch (event.type) {
        case "focus":
            this._handleWindowFocus(event);
            break;
        case "blur":
            this._handleWindowBlur(event);
            break;
        case "keydown":
            this._handleWindowKeyDown(event);
            break;
        case "mousedown":
            this._handleWindowMouseDown(event);
            break;
        }
    }

    // Private

    _didInteractWithTabContent()
    {
        if (!this._shouldLogTabContentActivity)
            return;

        let selectedTabContentView = WI.tabBrowser.selectedTabContentView;
        console.assert(selectedTabContentView);
        if (!selectedTabContentView)
            return;

        let tabType = selectedTabContentView.type;
        let interval = TabActivityDiagnosticEventRecorder.ActivityTimingResolution;
        this.logDiagnosticEvent(this.name, {tabType, interval});

        this._beginTabActivityTimeout();
    }

    _clearTabActivityTimeout()
    {
        if (this._tabActivityTimeoutIdentifier) {
            clearTimeout(this._tabActivityTimeoutIdentifier);
            this._tabActivityTimeoutIdentifier = 0;
        }
    }

    _beginTabActivityTimeout()
    {
        this._stopTrackingTabActivity();

        this._tabActivityTimeoutIdentifier = setTimeout(() => {
            this._shouldLogTabContentActivity = true;
            this._tabActivityTimeoutIdentifier = 0;
        }, TabActivityDiagnosticEventRecorder.ActivityTimingResolution);
    }

    _stopTrackingTabActivity()
    {
        this._clearTabActivityTimeout();
        this._shouldLogTabContentActivity = false;
    }

    _handleWindowFocus(event)
    {
        if (event.target !== window)
            return;

        this._shouldLogTabContentActivity = true;
    }

    _handleWindowBlur(event)
    {
        if (event.target !== window)
            return;

        this._stopTrackingTabActivity();
    }

    _handleWindowKeyDown(event)
    {
        this._didInteractWithTabContent();
    }

    _handleWindowMouseDown(event)
    {
        this._didInteractWithTabContent();
    }

    _handleTabBrowserSelectedTabContentViewDidChange(event)
    {
        this._clearTabActivityTimeout();
        this._shouldLogTabContentActivity = true;
    }
};

WI.TabActivityDiagnosticEventRecorder.ActivityTimingResolution = 60 * 1000;
