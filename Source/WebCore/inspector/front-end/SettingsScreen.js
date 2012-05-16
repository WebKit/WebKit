/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

/**
 * @constructor
 * @param {!function()} onHide
 * @extends {WebInspector.HelpScreen}
 */
WebInspector.SettingsScreen = function(onHide)
{
    WebInspector.HelpScreen.call(this, WebInspector.UIString("Settings"));
    this.contentElement.id = "settings";

    /** @type {!function()} */
    this._onHide = onHide;

    var container = document.createElement("div");
    container.className = "help-container";

    /**
     *  @param {string} name
     *  @return {!Element}
     */
    function appendSection(name) {
        var block = container.createChild("div", "help-block");
        block.createChild("div", "help-section-title").textContent = name;
        return block;
    }

    var p = appendSection(WebInspector.UIString("General"));
    if (Preferences.showDockToRight)
        p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Dock to right"), WebInspector.settings.dockToRight));
    if (Preferences.exposeDisableCache)
        p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Disable cache"), WebInspector.settings.cacheDisabled));
    var disableJSElement = this._createCheckboxSetting(WebInspector.UIString("Disable JavaScript"), WebInspector.settings.javaScriptDisabled);
    p.appendChild(disableJSElement);
    WebInspector.settings.javaScriptDisabled.addChangeListener(this._javaScriptDisabledChanged, this);
    this._disableJSCheckbox = disableJSElement.getElementsByTagName("input")[0];
    this._updateScriptDisabledCheckbox();
    
    p = appendSection(WebInspector.UIString("Rendering"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show paint rectangles"), WebInspector.settings.showPaintRects));
    WebInspector.settings.showPaintRects.addChangeListener(this._showPaintRectsChanged, this);

    p = appendSection(WebInspector.UIString("Elements"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Word wrap"), WebInspector.settings.domWordWrap));

    p = appendSection(WebInspector.UIString("Styles"));
    p.appendChild(this._createRadioSetting(WebInspector.UIString("Color format"), [
        [ WebInspector.StylesSidebarPane.ColorFormat.Original, WebInspector.UIString("As authored") ],
        [ WebInspector.StylesSidebarPane.ColorFormat.HEX, "HEX: #DAC0DE" ],
        [ WebInspector.StylesSidebarPane.ColorFormat.RGB, "RGB: rgb(128, 255, 255)" ],
        [ WebInspector.StylesSidebarPane.ColorFormat.HSL, "HSL: hsl(300, 80%, 90%)" ] ], WebInspector.settings.colorFormat));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show user agent styles"), WebInspector.settings.showUserAgentStyles));

    p = appendSection(WebInspector.UIString("Text editor"));
    p.appendChild(this._createSelectSetting(WebInspector.UIString("Indent"), [
            [ WebInspector.UIString("2 spaces"), WebInspector.TextEditorModel.Indent.TwoSpaces ],
            [ WebInspector.UIString("4 spaces"), WebInspector.TextEditorModel.Indent.FourSpaces ],
            [ WebInspector.UIString("8 spaces"), WebInspector.TextEditorModel.Indent.EightSpaces ],
            [ WebInspector.UIString("Tab character"), WebInspector.TextEditorModel.Indent.TabCharacter ]
        ], WebInspector.settings.textEditorIndent));

    p = appendSection(WebInspector.UIString("User Agent"));
    p.appendChild(this._createUserAgentControl());
    if (Capabilities.canOverrideDeviceMetrics)
        p.appendChild(this._createDeviceMetricsControl());
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Emulate touch events"), WebInspector.settings.emulateTouchEvents));

    p = appendSection(WebInspector.UIString("Scripts"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show script folders"), WebInspector.settings.showScriptFolders));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Search in content scripts"), WebInspector.settings.searchInContentScripts));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Enable source maps"), WebInspector.settings.sourceMapsEnabled));

    p = appendSection(WebInspector.UIString("Profiler"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show objects' hidden properties"), WebInspector.settings.showHeapSnapshotObjectsHiddenProperties));

    p = appendSection(WebInspector.UIString("Console"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Log XMLHttpRequests"), WebInspector.settings.monitoringXHREnabled));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Preserve log upon navigation"), WebInspector.settings.preserveConsoleLog));

    if (WebInspector.extensionServer.hasExtensions()) {
        var handlerSelector = new WebInspector.HandlerSelector(WebInspector.openAnchorLocationRegistry);
        p = appendSection(WebInspector.UIString("Extensions"));
        p.appendChild(this._createCustomSetting(WebInspector.UIString("Open links in"), handlerSelector.element));
    }

    var experiments = WebInspector.experimentsSettings.experiments;
    if (WebInspector.experimentsSettings.experimentsEnabled && experiments.length) {
        var experimentsSection = appendSection(WebInspector.UIString("Experiments"));
        experimentsSection.appendChild(this._createExperimentsWarningSubsection());
        for (var i = 0; i < experiments.length; ++i)
            experimentsSection.appendChild(this._createExperimentCheckbox(experiments[i]));
    }

    this.contentElement.appendChild(container);
}

WebInspector.SettingsScreen.prototype = {

    /**
     * @override
     */
    isClosingKey: function(keyCode)
    {
        return [
            WebInspector.KeyboardShortcut.Keys.Enter.code,
            WebInspector.KeyboardShortcut.Keys.Esc.code,
        ].indexOf(keyCode) >= 0;
    },

    /**
     * @override
     */
    willHide: function()
    {
        this._onHide();
        WebInspector.HelpScreen.prototype.willHide.call(this);
    },

    /**
     * @return {Element} element
     */
    _createExperimentsWarningSubsection: function()
    {
        var subsection = document.createElement("div");
        var warning = subsection.createChild("span", "settings-experiments-warning-subsection-warning");
        warning.textContent = WebInspector.UIString("WARNING:");
        subsection.appendChild(document.createTextNode(" "));
        var message = subsection.createChild("span", "settings-experiments-warning-subsection-message");
        message.textContent = WebInspector.UIString("These experiments could be dangerous and may require restart.");
        return subsection;
    },

    /**
     * @param {boolean=} omitParagraphElement
     */
    _createCheckboxSetting: function(name, setting, omitParagraphElement)
    {
        var input = document.createElement("input");
        input.type = "checkbox";
        input.name = name;
        input.checked = setting.get();

        function listener()
        {
            setting.set(input.checked);
        }
        input.addEventListener("click", listener, false);

        var label = document.createElement("label");
        label.appendChild(input);
        label.appendChild(document.createTextNode(name));
        if (omitParagraphElement)
            return label;

        var p = document.createElement("p");
        p.appendChild(label);
        return p;
    },

    _createExperimentCheckbox: function(experiment)
    {
        var input = document.createElement("input");
        input.type = "checkbox";
        input.name = experiment.name;
        input.checked = experiment.isEnabled();
        function listener()
        {
            experiment.setEnabled(input.checked);
        }
        input.addEventListener("click", listener, false);

        var p = document.createElement("p");
        var label = document.createElement("label");
        label.appendChild(input);
        label.appendChild(document.createTextNode(WebInspector.UIString(experiment.title)));
        p.appendChild(label);
        return p;
    },

    _createSelectSetting: function(name, options, setting)
    {
        var fieldsetElement = document.createElement("fieldset");
        fieldsetElement.createChild("label").textContent = name;

        var select = document.createElement("select");
        var settingValue = setting.get();

        for (var i = 0; i < options.length; ++i) {
            var option = options[i];
            select.add(new Option(option[0], option[1]));
            if (settingValue === option[1])
                select.selectedIndex = i;
        }

        function changeListener(e)
        {
            setting.set(e.target.value);
        }

        select.addEventListener("change", changeListener, false);
        fieldsetElement.appendChild(select);

        var p = document.createElement("p");
        p.appendChild(fieldsetElement);
        return p;
    },

    _createRadioSetting: function(name, options, setting)
    {
        var pp = document.createElement("p");
        var fieldsetElement = document.createElement("fieldset");
        var legendElement = document.createElement("legend");
        legendElement.textContent = name;
        fieldsetElement.appendChild(legendElement);

        function clickListener(e)
        {
            setting.set(e.target.value);
        }

        var settingValue = setting.get();
        for (var i = 0; i < options.length; ++i) {
            var p = document.createElement("p");
            var label = document.createElement("label");
            p.appendChild(label);

            var input = document.createElement("input");
            input.type = "radio";
            input.name = setting.name;
            input.value = options[i][0];
            input.addEventListener("click", clickListener, false);
            if (settingValue == input.value)
                input.checked = true;

            label.appendChild(input);
            label.appendChild(document.createTextNode(options[i][1]));

            fieldsetElement.appendChild(p);
        }

        pp.appendChild(fieldsetElement);
        return pp;
    },

    _createCustomSetting: function(name, element)
    {
        var p = document.createElement("p");
        var fieldsetElement = document.createElement("fieldset");
        fieldsetElement.createChild("label").textContent = name;
        fieldsetElement.appendChild(element);
        p.appendChild(fieldsetElement);
        return p;
    },

    _createUserAgentControl: function()
    {
        var userAgent = WebInspector.settings.userAgent.get();

        var p = document.createElement("p");
        var labelElement = p.createChild("label");
        var checkboxElement = labelElement.createChild("input");
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !!userAgent;
        checkboxElement.addEventListener("click", checkboxClicked.bind(this), false);
        labelElement.appendChild(document.createTextNode("Override User Agent"));

        var selectSectionElement;
        function checkboxClicked()
        {
            if (checkboxElement.checked) {
                selectSectionElement = this._createUserAgentSelectRowElement();
                p.appendChild(selectSectionElement);
            } else {
                if (selectSectionElement) {
                    p.removeChild(selectSectionElement);
                    selectSectionElement = null;
                }
                WebInspector.settings.userAgent.set("");
            }
        }

        checkboxClicked.call(this);
        return p;
    },

    _createUserAgentSelectRowElement: function()
    {
        var userAgent = WebInspector.settings.userAgent.get();

        // When present, the third element lists device metrics separated by 'x':
        // - screen width,
        // - screen height,
        // - font scale factor.
        const userAgents = [
            ["Internet Explorer 9", "Mozilla/5.0 (compatible; MSIE 9.0; Windows NT 6.1; Trident/5.0)"],
            ["Internet Explorer 8", "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.0; Trident/4.0)"],
            ["Internet Explorer 7", "Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0)"],

            ["Firefox 7 \u2014 Windows", "Mozilla/5.0 (Windows NT 6.1; Intel Mac OS X 10.6; rv:7.0.1) Gecko/20100101 Firefox/7.0.1"],
            ["Firefox 7 \u2014 Mac", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.6; rv:7.0.1) Gecko/20100101 Firefox/7.0.1"],
            ["Firefox 4 \u2014 Windows", "Mozilla/5.0 (Windows NT 6.1; rv:2.0.1) Gecko/20100101 Firefox/4.0.1"],
            ["Firefox 4 \u2014 Mac", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10.6; rv:2.0.1) Gecko/20100101 Firefox/4.0.1"],

            ["iPhone \u2014 iOS 5", "Mozilla/5.0 (iPhone; CPU iPhone OS 5_0 like Mac OS X) AppleWebKit/534.46 (KHTML, like Gecko) Version/5.1 Mobile/9A334 Safari/7534.48.3", "640x960x1"],
            ["iPhone \u2014 iOS 4", "Mozilla/5.0 (iPhone; U; CPU iPhone OS 4_3_2 like Mac OS X; en-us) AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8H7 Safari/6533.18.5", "640x960x1"],
            ["iPad \u2014 iOS 5", "Mozilla/5.0 (iPad; CPU OS 5_0 like Mac OS X) AppleWebKit/534.46 (KHTML, like Gecko) Version/5.1 Mobile/9A334 Safari/7534.48.3", "1024x768x1"],
            ["iPad \u2014 iOS 4", "Mozilla/5.0 (iPad; CPU OS 4_3_2 like Mac OS X; en-us) AppleWebKit/533.17.9 (KHTML, like Gecko) Version/5.0.2 Mobile/8H7 Safari/6533.18.5", "1024x768x1"],

            ["Android 2.3 \u2014 Nexus S", "Mozilla/5.0 (Linux; U; Android 2.3.6; en-us; Nexus S Build/GRK39F) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1", "480x800x1.1"],
            ["Android 4.0.2 \u2014 Galaxy Nexus", "Mozilla/5.0 (Linux; U; Android 4.0.2; en-us; Galaxy Nexus Build/ICL53F) AppleWebKit/534.30 (KHTML, like Gecko) Version/4.0 Mobile Safari/534.30", "720x1280x1.1"],

            ["BlackBerry \u2014 PlayBook 1.0","Mozilla/5.0 (PlayBook; U; RIM Tablet OS 1.0.0; en-US) AppleWebKit/534.11+ (KHTML, like Gecko) Version/7.1.0.7 Safari/534.11+", "1024x600x1"],
            ["BlackBerry \u2014 PlayBook 2.0", "Mozilla/5.0 (PlayBook; U; RIM Tablet OS 2.0.0; en-US) AppleWebKit/535.8+ (KHTML, like Gecko) Version/7.2.0.0 Safari/535.8+", "1024x600x1"],
            ["BlackBerry \u2014 9900", "Mozilla/5.0 (BlackBerry; U; BlackBerry 9900; en-US) AppleWebKit/534.11+ (KHTML, like Gecko) Version/7.0.0.187 Mobile Safari/534.11+", "640x480x1"],

            ["MeeGo \u2014 Nokia N9", "Mozilla/5.0 (MeeGo; NokiaN9) AppleWebKit/534.13 (KHTML, like Gecko) NokiaBrowser/8.5.0 Mobile Safari/534.13", "480x854x1"],

            [WebInspector.UIString("Other..."), "Other"]
        ];

        var fieldsetElement = document.createElement("fieldset");
        var selectElement = fieldsetElement.createChild("select");
        var otherUserAgentElement = fieldsetElement.createChild("input");
        otherUserAgentElement.value = userAgent;
        otherUserAgentElement.title = userAgent;

        var selectionRestored = false;
        for (var i = 0; i < userAgents.length; ++i) {
            var agent = userAgents[i];
            var option = new Option(agent[0], agent[1]);
            option._metrics = agent[2] ? agent[2] : "";
            selectElement.add(option);
            if (userAgent === agent[1]) {
                selectElement.selectedIndex = i;
                selectionRestored = true;
            }
        }

        if (!selectionRestored) {
            if (!userAgent)
                selectElement.selectedIndex = 0;
            else
                selectElement.selectedIndex = userAgents.length - 1;
        }

        selectElement.addEventListener("change", selectionChanged.bind(this, true), false);

        /**
         * @param {boolean=} isUserGesture
         */
        function selectionChanged(isUserGesture)
        {
            var value = selectElement.options[selectElement.selectedIndex].value;
            if (value !== "Other") {
                WebInspector.settings.userAgent.set(value);
                otherUserAgentElement.value = value;
                otherUserAgentElement.title = value;
                otherUserAgentElement.disabled = true;
            } else {
                otherUserAgentElement.disabled = false;
                otherUserAgentElement.focus();
            }

            if (isUserGesture && Capabilities.canOverrideDeviceMetrics) {
                var metrics = selectElement.options[selectElement.selectedIndex]._metrics;
                this._setDeviceMetricsOverride(WebInspector.UserAgentSupport.DeviceMetrics.parseSetting(metrics), false, true);
            }
        }

        fieldsetElement.addEventListener("dblclick", textDoubleClicked.bind(this), false);
        otherUserAgentElement.addEventListener("blur", textChanged.bind(this), false);

        function textDoubleClicked()
        {
            selectElement.selectedIndex = userAgents.length - 1;
            selectionChanged.call(this);
        }

        function textChanged()
        {
            WebInspector.settings.userAgent.set(otherUserAgentElement.value);
        }

        selectionChanged.call(this);
        return fieldsetElement;
    },

    _showPaintRectsChanged: function()
    {
        PageAgent.setShowPaintRects(WebInspector.settings.showPaintRects.get());
    },

    _updateScriptDisabledCheckbox: function()
    {
        function executionStatusCallback(error, status)
        {
            if (error || !status)
                return;

            switch (status) {
            case "forbidden":
                this._disableJSCheckbox.checked = true;
                this._disableJSCheckbox.disabled = true;
                break;
            case "disabled":
                this._disableJSCheckbox.checked = true;
                break;
            default:
                this._disableJSCheckbox.checked = false;
                break;
            }
        }

        PageAgent.getScriptExecutionStatus(executionStatusCallback.bind(this));
    },

    _javaScriptDisabledChanged: function()
    {
        // We need to manually update the checkbox state, since enabling JavaScript in the page can actually uncover the "forbidden" state.
        PageAgent.setScriptExecutionDisabled(WebInspector.settings.javaScriptDisabled.get(), this._updateScriptDisabledCheckbox.bind(this));
    },

    _createDeviceMetricsControl: function()
    {
        const metricsSetting = WebInspector.settings.deviceMetrics.get();
        var metrics = WebInspector.UserAgentSupport.DeviceMetrics.parseSetting(metricsSetting);

        const p = document.createElement("p");
        const labelElement = p.createChild("label");
        const checkboxElement = labelElement.createChild("input");
        checkboxElement.id = "metrics-override-checkbox";
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !metrics || (metrics.width && metrics.height && metrics.fontScaleFactor);
        checkboxElement.addEventListener("click", this._onMetricsCheckboxClicked.bind(this), false);
        this._metricsCheckboxElement = checkboxElement;
        labelElement.appendChild(document.createTextNode(WebInspector.UIString("Override device metrics")));

        const metricsSectionElement = this._createDeviceMetricsElement(metrics);
        p.appendChild(metricsSectionElement);
        this._metricsSectionElement = metricsSectionElement;

        this._setDeviceMetricsOverride(metrics, false, true);

        return p;
    },

    _onMetricsCheckboxClicked: function()
    {
        if (this._metricsCheckboxElement.checked) {
            this._metricsSectionElement.removeStyleClass("hidden");
            var metrics = WebInspector.UserAgentSupport.DeviceMetrics.parseUserInput(this._widthOverrideElement.value, this._heightOverrideElement.value, this._fontScaleFactorOverrideElement.value);
            if (metrics && metrics.isValid() && metrics.width && metrics.height)
                this._setDeviceMetricsOverride(metrics, false, false);
            if (!this._widthOverrideElement.value)
                this._widthOverrideElement.focus();
        } else {
            this._metricsSectionElement.addStyleClass("hidden");
            if (WebInspector.settings.deviceMetrics.get())
                WebInspector.settings.deviceMetrics.set("");
        }
    },

    _applyDeviceMetricsUserInput: function()
    {
        this._setDeviceMetricsOverride(WebInspector.UserAgentSupport.DeviceMetrics.parseUserInput(this._widthOverrideElement.value.trim(), this._heightOverrideElement.value.trim(), this._fontScaleFactorOverrideElement.value.trim()), true, false);
    },

    /**
     * @param {?WebInspector.UserAgentSupport.DeviceMetrics} metrics
     * @param {boolean} userInputModified
     */
    _setDeviceMetricsOverride: function(metrics, userInputModified, updateCheckbox)
    {
        function setValid(condition, element)
        {
            if (condition)
                element.removeStyleClass("error-input");
            else
                element.addStyleClass("error-input");
        }

        setValid(metrics && metrics.isWidthValid(), this._widthOverrideElement);
        setValid(metrics && metrics.isHeightValid(), this._heightOverrideElement);
        setValid(metrics && metrics.isFontScaleFactorValid(), this._fontScaleFactorOverrideElement);

        if (!metrics)
            return;

        if (!userInputModified) {
            this._widthOverrideElement.value = metrics.widthToInput();
            this._heightOverrideElement.value = metrics.heightToInput();
            this._fontScaleFactorOverrideElement.value = metrics.fontScaleFactorToInput();
        }

        if (metrics.isValid()) {
            var value = metrics.toSetting();
            if (value !== WebInspector.settings.deviceMetrics.get())
                WebInspector.settings.deviceMetrics.set(value);
        }

        if (this._metricsCheckboxElement && updateCheckbox) {
            this._metricsCheckboxElement.checked = !!metrics.toSetting();
            this._onMetricsCheckboxClicked();
        }
    },

    /**
     * @param {WebInspector.UserAgentSupport.DeviceMetrics} metrics
     */
    _createDeviceMetricsElement: function(metrics)
    {
        var fieldsetElement = document.createElement("fieldset");
        fieldsetElement.id = "metrics-override-section";

        function createInput(parentElement, id, defaultText)
        {
            var element = parentElement.createChild("input");
            element.id = id;
            element.maxLength = 6;
            element.style.width = "48px";
            element.value = defaultText;
            element.addEventListener("blur", this._applyDeviceMetricsUserInput.bind(this), false);
            return element;
        }

        function swapDimensionsClicked(event)
        {
            var widthValue = this._widthOverrideElement.value;
            this._widthOverrideElement.value = this._heightOverrideElement.value;
            this._heightOverrideElement.value = widthValue;
            this._applyDeviceMetricsUserInput();
        }

        var tableElement = fieldsetElement.createChild("table");

        var rowElement = tableElement.createChild("tr");
        var cellElement = rowElement.createChild("td");
        cellElement.appendChild(document.createTextNode(WebInspector.UIString("Screen resolution:")));
        cellElement = rowElement.createChild("td");
        this._widthOverrideElement = createInput.call(this, cellElement, "metrics-override-width", String(metrics.width || screen.width));
        cellElement.appendChild(document.createTextNode(" \u00D7 ")); // MULTIPLICATION SIGN.
        this._heightOverrideElement = createInput.call(this, cellElement, "metrics-override-height", String(metrics.height || screen.height));
        cellElement.appendChild(document.createTextNode(" \u2014 ")); // EM DASH.
        var swapDimensionsElement = cellElement.createChild("button");
        swapDimensionsElement.appendChild(document.createTextNode(" \u21C4 ")); // RIGHTWARDS ARROW OVER LEFTWARDS ARROW.
        swapDimensionsElement.title = WebInspector.UIString("Swap dimensions");
        swapDimensionsElement.addEventListener("click", swapDimensionsClicked.bind(this), false);

        rowElement = tableElement.createChild("tr");
        cellElement = rowElement.createChild("td");
        cellElement.appendChild(document.createTextNode(WebInspector.UIString("Font scale factor:")));
        cellElement = rowElement.createChild("td");
        this._fontScaleFactorOverrideElement = createInput.call(this, cellElement, "metrics-override-font-scale", String(metrics.fontScaleFactor || 1));

        rowElement = tableElement.createChild("tr");
        cellElement = rowElement.createChild("td");
        cellElement.colspan = 2;
        cellElement.appendChild(this._createCheckboxSetting(WebInspector.UIString("Fit window"), WebInspector.settings.deviceFitWindow, true));

        return fieldsetElement;
    }
}

WebInspector.SettingsScreen.prototype.__proto__ = WebInspector.HelpScreen.prototype;

/**
 * @constructor
 */
WebInspector.SettingsController = function()
{
    this._statusBarButton = new WebInspector.StatusBarButton(WebInspector.UIString("Settings"), "settings-status-bar-item");;
    this._statusBarButton.addEventListener("click", this._buttonClicked, this);

    /** @type {?WebInspector.SettingsScreen} */
    this._settingsScreen;
}

WebInspector.SettingsController.prototype =
{
    get statusBarItem()
    {
        return this._statusBarButton.element;
    },

    _buttonClicked: function()
    {
        if (this._statusBarButton.toggled)
            this._hideSettingsScreen();
        else
            this._showSettingsScreen();
    },

    _onHideSettingsScreen: function()
    {
        this._statusBarButton.toggled = false;
        delete this._settingsScreen;
    },

    _showSettingsScreen: function()
    {
        if (!this._settingsScreen)
            this._settingsScreen = new WebInspector.SettingsScreen(this._onHideSettingsScreen.bind(this));

        this._settingsScreen.showModal();
        this._statusBarButton.toggled = true;
    },

    _hideSettingsScreen: function()
    {
        if (this._settingsScreen)
            this._settingsScreen.hide();
    }
}
