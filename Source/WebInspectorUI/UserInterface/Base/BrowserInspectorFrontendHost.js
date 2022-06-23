/*
 * Copyright (C) 2022 Igalia S.L.
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

if (!window.InspectorFrontendHost) {
    WI.BrowserInspectorFrontendHost = class BrowserInspectorFrontendHost {

        constructor()
        {
            this._pendingMessages = null;
            this._socket = null;
        }

        // Public

        get supportsShowCertificate()
        {
            return false;
        }

        get isRemote()
        {
            return true;
        }

        get inspectionLevel()
        {
            return 1;
        }

        get debuggableInfo()
        {
            return {
                debuggableType: "web-page",
                targetPlatformName: undefined,
                targetBuildVersion: undefined,
                targetProductVersion: undefined,
                targetIsSimulator: false,
            };
        }

        get platform()
        {
            const match = navigator.platform.match(/mac|win|linux/i);
            if (match) {
                const platform = match[0].toLowerCase();
                if (platform == "win")
                    return "windows";
                return platform;
            }
            return "unknown";
        }

        get platformVersionName()
        {
            return "";
        }

        get supportsDiagnosticLogging()
        {
            return false;
        }

        get supportsWebExtensions()
        {
            return false;
        }

        connect()
        {
            const queryParams = parseQueryString(window.location.search.substring(1));
            let url = "ws" in queryParams ? "ws://" + queryParams.ws : null;
            if (!url)
                return;

            const socket = new WebSocket(url);
            socket.addEventListener("message", message => InspectorBackend.dispatch(message.data));
            socket.addEventListener("error", console.error);
            socket.addEventListener("open", () => { this._socket = socket; });
            socket.addEventListener("close", () => {
                this._socket = null;
                window.close();
            });
        }

        loaded()
        {
            WI.updateVisibilityState(true);
        }

        closeWindow()
        {
            this._windowVisible = false;
        }

        reopen()
        {
            window.location.reload();
        }

        reset()
        {
            this.reopen();
        }

        bringToFront()
        {
            this._windowVisible = true;
        }

        inspectedURLChanged(title)
        {
            document.title = title;
        }

        showCertificate(certificate)
        {
            throw "unimplemented";
        }

        setZoomFactor(zoom)
        {
        }

        zoomFactor()
        {
            return 1;
        }

        setForcedAppearance(appearance)
        {
        }

        userInterfaceLayoutDirection()
        {
            return "ltr";
        }

        supportsDockSide(side)
        {
            return false;
        }

        requestDockSide(side)
        {
            throw "unimplemented";
        }

        setAttachedWindowHeight(height)
        {
        }

        setAttachedWindowWidth(width)
        {
        }

        setSheetRect(x, y, width, height)
        {
        }

        startWindowDrag()
        {
        }

        moveWindowBy(x, y)
        {
        }

        copyText(text)
        {
            this.killText(text, false, true);
        }

        killText(text, shouldPrependToKillRing, shouldStartNewSequence)
        {
            // FIXME: restore focus to previously focused element.
            let textarea = document.createElement("textarea");
            document.body.appendChild(textarea);

            if (shouldStartNewSequence) {
                textarea.textContent = text;
            } else {
                textarea.select();
                if (!document.execCommand("paste"))
                    console.error("BrowserInspectorFrontendHost.killText: could not paste from clipboard");

                if (shouldPrependToKillRing)
                    textarea.textContent = text + textarea.textContent;
                else
                    textarea.textContent = textarea.textContent + text;
            }

            textarea.select();

            if (!document.execCommand("copy"))
                console.error("BrowserInspectorFrontendHost.copyText: could not copy to clipboard");

            document.body.removeChild(textarea);
        }

        openURLExternally(url)
        {
            window.open(url, "_blank");
        }

        canSave(saveMode)
        {
            return false;
        }

        save(saveDatas, forceSaveAs)
        {
            // FIXME: Create a Blob from the content, get an object URL, open it to trigger a download.
            throw "unimplemented";
        }

        canLoad()
        {
            return false;
        }

        load(path)
        {
            throw "unimplemented";
        }

        getPath(file)
        {
            return null;
        }

        canPickColorFromScreen()
        {
            return false;
        }

        pickColorFromScreen()
        {
            throw "unimplemented";
        }

        revealFileExternally(path)
        {
        }

        getCurrentX(context)
        {
            return 0.0;
        }

        getCurrentY(context)
        {
            return 0.0;
        }

        setPath(context, path2d)
        {
            console.log("setPath", context, path2d);
        }

        showContextMenu(event, items)
        {
            this._contextMenu = WI.SoftContextMenu(items);
            this._contextMenu.show(event);
        }

        dispatchEventAsContextMenuEvent(event)
        {
            if (this._contextMenu)
                this._contextMenu.show(event);
        }

        sendMessageToBackend(message)
        {
            if (!this._socket) {
                if (!this._pendingMessages)
                    this._pendingMessages = [];
                this._pendingMessages.push(message);
            } else {
                this._sendPendingMessagesToBackendIfNeeded();
                this._socket.send(message);
            }
        }

        unbufferedLog(message)
        {
            console.log(message);
        }

        isUnderTest()
        {
            return false;
        }

        beep()
        {
            // FIXME: Implement using Audio/AudioContext.
        }

        inspectInspector()
        {
            throw "unimplemented";
        }

        isBeingInspected()
        {
            return false;
        }

        setAllowsInspectingInspector(allow)
        {
        }

        logDiagnosticEvent(eventName, content)
        {
            throw "unimplemented";
        }

        didShowExtensionTab(extensionID, extensionTabID, extensionFrame)
        {
            throw "unimplemented";
        }

        didHideExtensionTab(extensionID, extensionTabID)
        {
            throw "unimplemented";
        }

        didNavigateExtensionTab(extensionID, extensionTabID, newURL)
        {
            throw "unimplemented";
        }

        inspectedPageDidNavigate(newURL)
        {
            throw "unimplemented";
        }

        evaluateScriptInExtensionTab(extensionFrame, scriptSource)
        {
            throw "unimplemented";
        }

        // Private

        _sendPendingMessagesToBackendIfNeeded()
        {
            if (this._pendingMessages) {
                this._pendingMessages.forEach(message => this._socket.send(message));
                this._pendingMessages = null;
            }
        }
    };

    InspectorFrontendHost = new WI.BrowserInspectorFrontendHost();

    WI.dontLocalizeUserInterface = true;
}
