/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

WI.ResourceContentView = class ResourceContentView extends WI.ContentView
{
    constructor(resource, styleClassName)
    {
        console.assert(resource instanceof WI.Resource || resource instanceof WI.CSSStyleSheet, resource);
        console.assert(typeof styleClassName === "string");

        super(resource);

        this._resource = resource;

        this.element.classList.add(styleClassName, "resource");

        this._spinnerTimeout = setTimeout(() => {
            // Append a spinner while waiting for contentAvailable. Subclasses are responsible for
            // removing the spinner before showing the resource content by calling removeLoadingIndicator.
            let spinner = new WI.IndeterminateProgressSpinner;
            this.element.appendChild(spinner.element);

            this._spinnerTimeout = undefined;
        }, 100);

        this.element.addEventListener("click", this._mouseWasClicked.bind(this), false);

        // Request content last so the spinner will always be removed in case the content is immediately available.
        resource.requestContent().then(this._contentAvailable.bind(this)).catch(this.showGenericErrorMessage.bind(this));

        if (!this.managesOwnIssues) {
            WI.consoleManager.addEventListener(WI.ConsoleManager.Event.IssueAdded, this._issueWasAdded, this);

            var issues = WI.consoleManager.issuesForSourceCode(resource);
            for (var i = 0; i < issues.length; ++i)
                this.addIssue(issues[i]);
        }

        this._showingLocalResourceOverride = false;

        if (WI.NetworkManager.supportsOverridingResponses()) {
            if (resource.isLocalResourceOverride) {
                this._showingLocalResourceOverride = true;

                this._localResourceOverrideBannerView = new WI.LocalResourceOverrideLabelView(resource);

                this._importLocalResourceOverrideButtonNavigationItem = new WI.ButtonNavigationItem("import-local-resource-override", WI.UIString("Import"), "Images/Import.svg", 15, 15);
                this._importLocalResourceOverrideButtonNavigationItem.buttonStyle = WI.ButtonNavigationItem.Style.ImageAndText;
                this._importLocalResourceOverrideButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
                this._importLocalResourceOverrideButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleImportLocalResourceOverride, this);

                this._removeLocalResourceOverrideButtonNavigationItem = new WI.ButtonNavigationItem("remove-local-resource-override", WI.UIString("Delete Local Override"), "Images/NavigationItemTrash.svg", 15, 15);
                this._removeLocalResourceOverrideButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleRemoveLocalResourceOverride, this);
                this._removeLocalResourceOverrideButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
            } else {
                this._localResourceOverrideBannerView = new WI.LocalResourceOverrideWarningView(resource);

                this._createLocalResourceOverrideButtonNavigationItem = new WI.ButtonNavigationItem("create-local-resource-override", this.createLocalResourceOverrideTooltip, "Images/NavigationItemNetworkOverride.svg", 13, 14);
                this._createLocalResourceOverrideButtonNavigationItem.addEventListener(WI.ButtonNavigationItem.Event.Clicked, this._handleCreateLocalResourceOverride, this);
                this._createLocalResourceOverrideButtonNavigationItem.enabled = false; // Enabled when the content is available.
                this._createLocalResourceOverrideButtonNavigationItem.visibilityPriority = WI.NavigationItem.VisibilityPriority.Low;
            }

            WI.networkManager.addEventListener(WI.NetworkManager.Event.LocalResourceOverrideAdded, this._handleLocalResourceOverrideChanged, this);
            WI.networkManager.addEventListener(WI.NetworkManager.Event.LocalResourceOverrideRemoved, this._handleLocalResourceOverrideChanged, this);
        }
    }

    // Public

    get resource() { return this._resource; }
    get showingLocalResourceOverride() { return this._showingLocalResourceOverride; }

    get navigationItems()
    {
        let items = [];

        if (this._importLocalResourceOverrideButtonNavigationItem)
            items.push(this._importLocalResourceOverrideButtonNavigationItem, new WI.DividerNavigationItem);
        if (this._removeLocalResourceOverrideButtonNavigationItem)
            items.push(this._removeLocalResourceOverrideButtonNavigationItem);
        if (this._createLocalResourceOverrideButtonNavigationItem)
            items.push(this._createLocalResourceOverrideButtonNavigationItem);

        return items;
    }

    get supportsSave()
    {
        return this._resource.finished;
    }

    get saveData()
    {
        let saveData = {
            url: this._resource.url,
            content: this._resource.content,
        };

        if (this._resource.urlComponents.path === "/") {
            let extension = WI.fileExtensionForMIMEType(this._resource.mimeType);
            if (extension)
                saveData.suggestedName = `index.${extension}`;
        }

        return saveData;
    }

    contentAvailable(content, base64Encoded)
    {
        throw WI.NotImplementedError.subclassMustOverride();
    }

    get createLocalResourceOverrideTooltip()
    {
        return WI.UIString("Click to import a file and create a Local Override\nShift-click to create a Local Override from this content");
    }

    requestLocalResourceOverrideInitialContent(callback)
    {
        // Overridden by subclasses if needed.

        WI.FileUtilities.import(async (fileList) => {
            console.assert(fileList.length === 1);

            this._getContentForLocalResourceOverrideFromFile(fileList[0], ({mimeType, base64Encoded, content}) => {
                callback({
                    initialMIMEType: mimeType,
                    initialBase64Encoded: base64Encoded,
                    initialContent: content,
                });
            });
        });
    }

    showGenericNoContentMessage()
    {
        this.showMessage(WI.UIString("Resource has no content."));

        this.dispatchEventToListeners(WI.ResourceContentView.Event.ContentError);
    }
    
    showNoCachedContentMessage()
    {
        this.showMessage(WI.UIString("Resource has no cached content.", "Resource has no cached content. @ Resource Preview", "An error message shown when there is no cached content for a HTTP 304 Not Modified resource response."));

        this.dispatchEventToListeners(WI.ResourceContentView.Event.ContentError);
    }

    showGenericErrorMessage()
    {
        this._contentError(WI.UIString("An error occurred trying to load the resource."));
    }

    showMessage(message)
    {
        this.element.removeChildren();
        this.element.appendChild(WI.createMessageTextView(message));
    }

    addIssue(issue)
    {
        // This generically shows only the last issue, subclasses can override for better handling.
        this.element.removeChildren();
        this.element.appendChild(WI.createMessageTextView(issue.text, issue.level === WI.IssueMessage.Level.Error));
    }

    closed()
    {
        super.closed();

        if (WI.NetworkManager.supportsOverridingResponses())
            WI.networkManager.removeEventListener(null, null, this);

        if (!this.managesOwnIssues)
            WI.consoleManager.removeEventListener(null, null, this);
    }

    // Protected

    removeLoadingIndicator()
    {
        if (this._spinnerTimeout) {
            clearTimeout(this._spinnerTimeout);
            this._spinnerTimeout = undefined;
        }

        this.removeAllSubviews();

        if (this._localResourceOverrideBannerView)
            this.addSubview(this._localResourceOverrideBannerView);
    }

    // Private

    _contentAvailable(parameters)
    {
        if (parameters.error) {
            // A 304 Not Modified request that is missing content means we didn't have a cached copy.
            if (parameters.sourceCode.statusCode == 304 && parameters.reason === "Missing content of resource for given requestId") {
                this.showNoCachedContentMessage();
                return;
            }
            
            this._contentError(parameters.error);
            return;
        }

        if (parameters.message) {
            this.showMessage(parameters.message);
            return;
        }

        // The view maybe populated with inline scripts content by the time resource
        // content arrives. SourceCodeTextEditor will handle that.
        if (this._hasContent())
            return;
        
        if (!parameters.sourceCode.content && !parameters.sourceCode.mimeType) {
            this.showGenericNoContentMessage();
            return;
        }

        // Content is ready to show, call the public method now.
        console.assert(parameters.sourceCode === this._resource);
        this.contentAvailable(parameters.sourceCode.content, parameters.base64Encoded);

        if (this._createLocalResourceOverrideButtonNavigationItem)
            this._createLocalResourceOverrideButtonNavigationItem.enabled = WI.networkManager.canBeOverridden(this._resource);
    }

    _contentError(error)
    {
        if (this._hasContent())
            return;

        this.removeLoadingIndicator();

        this.element.appendChild(WI.createMessageTextView(error, true));

        this.dispatchEventToListeners(WI.ResourceContentView.Event.ContentError);
    }

    _hasContent()
    {
        return this.element.hasChildNodes() && !this.element.querySelector(".indeterminate-progress-spinner");
    }

    _issueWasAdded(event)
    {
        console.assert(!this.managesOwnIssues);

        var issue = event.data.issue;
        if (!WI.ConsoleManager.issueMatchSourceCode(issue, this._resource))
            return;

        this.addIssue(issue);
    }

    async _getContentForLocalResourceOverrideFromFile(file, callback)
    {
        let initialMIMEType = file.type || WI.mimeTypeForFileExtension(WI.fileExtensionForFilename(file.name));
        if (WI.shouldTreatMIMETypeAsText(initialMIMEType)) {
            await WI.FileUtilities.readText(file, async ({text}) => {
                await callback({
                    mimeType: initialMIMEType,
                    base64Encoded: false,
                    content: text,
                });
            });
        } else {
            await WI.FileUtilities.readData(file, async ({mimeType, base64Encoded, content}) => {
                await callback({mimeType, base64Encoded, content});
            });
        }
    }

    _handleCreateLocalResourceOverride(event)
    {
        let {nativeEvent} = event.data;

        let createLocalResourceOverride = async (initialContent) => {
            let localResourceOverride = await this._resource.createLocalResourceOverride(initialContent);
            WI.networkManager.addLocalResourceOverride(localResourceOverride);
            WI.showLocalResourceOverride(localResourceOverride);
        };

        if (nativeEvent.shiftKey)
            createLocalResourceOverride({});
        else
            this.requestLocalResourceOverrideInitialContent(createLocalResourceOverride);
    }

    _handleImportLocalResourceOverride(event)
    {
        console.assert(this._showingLocalResourceOverride);

        WI.FileUtilities.import(async (fileList) => {
            console.assert(fileList.length === 1);

            let localResourceOverride = WI.networkManager.localResourceOverrideForURL(this.resource.url);
            console.assert(localResourceOverride);

            await this._getContentForLocalResourceOverrideFromFile(fileList[0], ({mimeType, base64Encoded, content}) => {
                let revision = localResourceOverride.localResource.editableRevision;
                revision.updateRevisionContent(content, {base64Encoded, mimeType});
            });

            if (!this.showingLocalResourceOverride)
                WI.showLocalResourceOverride(localResourceOverride);
        });
    }

    _handleRemoveLocalResourceOverride(event)
    {
        console.assert(this._showingLocalResourceOverride);

        let localResourceOverride = WI.networkManager.localResourceOverrideForURL(this._resource.url);
        WI.networkManager.removeLocalResourceOverride(localResourceOverride);
    }

    _handleLocalResourceOverrideChanged(event)
    {
        if (this._resource.url !== event.data.localResourceOverride.url)
            return;

        if (this._createLocalResourceOverrideButtonNavigationItem)
            this._createLocalResourceOverrideButtonNavigationItem.enabled = WI.networkManager.canBeOverridden(this._resource);
    }

    _mouseWasClicked(event)
    {
        WI.handlePossibleLinkClick(event, this._resource.parentFrame);
    }
};

WI.ResourceContentView.Event = {
    ContentError: "resource-content-view-content-error",
};
