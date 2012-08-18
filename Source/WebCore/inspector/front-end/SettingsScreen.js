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
    WebInspector.HelpScreen.call(this);
    this.element.id = "settings-screen";

    /** @type {!function()} */
    this._onHide = onHide;

    this._tabbedPane = new WebInspector.TabbedPane();
    this._tabbedPane.element.addStyleClass("help-window-main");
    this._tabbedPane.element.appendChild(this._createCloseButton());
    this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.General, WebInspector.UIString("General"), new WebInspector.GenericSettingsTab());
    this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.UserAgent, WebInspector.UIString("Overrides"), new WebInspector.UserAgentSettingsTab());
    if (WebInspector.experimentsSettings.experimentsEnabled)
        this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.Experiments, WebInspector.UIString("Experiments"), new WebInspector.ExperimentsSettingsTab());
    this._tabbedPane.appendTab(WebInspector.SettingsScreen.Tabs.Shortcuts, WebInspector.UIString("Shortcuts"), WebInspector.shortcutsScreen.createShortcutsTabView());
    this._tabbedPane.shrinkableTabs = true;

    this._lastSelectedTabSetting = WebInspector.settings.createSetting("lastSelectedSettingsTab", WebInspector.SettingsScreen.Tabs.General);
    this.selectTab(this._lastSelectedTabSetting.get());
    this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabSelected, this._tabSelected, this);
}

WebInspector.SettingsScreen.Tabs = {
    General: "General",
    UserAgent: "UserAgent",
    Experiments: "Experiments",
    Shortcuts: "Shortcuts"
}

WebInspector.SettingsScreen.prototype = {
    /**
     * @param {!string} tabId
     */
    selectTab: function(tabId)
    {
        this._tabbedPane.selectTab(tabId);
    },

    /**
     * @param {WebInspector.Event} event
     */
    _tabSelected: function(event)
    {
        this._lastSelectedTabSetting.set(this._tabbedPane.selectedTabId);
    },

    /**
     * @override
     */
    wasShown: function()
    {
        this._tabbedPane.show(this.element);
        WebInspector.HelpScreen.prototype.wasShown.call(this);
    },

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
    }
}

WebInspector.SettingsScreen.prototype.__proto__ = WebInspector.HelpScreen.prototype;

/**
 * @constructor
 * @extends {WebInspector.View}
 */
WebInspector.SettingsTab = function()
{
    WebInspector.View.call(this);
    this.element.className = "settings-tab help-content help-container";
}

WebInspector.SettingsTab.prototype = {
    /**
     *  @param {string=} name
     *  @return {!Element}
     */
    _appendSection: function(name)
    {
        var block = this.element.createChild("div", "help-block");
        if (name)
            block.createChild("div", "help-section-title").textContent = name;
        return block;
    },

    /**
     * @param {boolean=} omitParagraphElement
     * @param {Element=} inputElement
     */
    _createCheckboxSetting: function(name, setting, omitParagraphElement, inputElement)
    {
        var input = inputElement || document.createElement("input");
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
    }
}

WebInspector.SettingsTab.prototype.__proto__ = WebInspector.View.prototype;

/**
 * @constructor
 * @extends {WebInspector.SettingsTab}
 */
WebInspector.GenericSettingsTab = function()
{
    WebInspector.SettingsTab.call(this);

    var p = this._appendSection();
    if (Preferences.showDockToRight)
        p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Dock to right"), WebInspector.settings.dockToRight));
    if (Preferences.exposeDisableCache)
        p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Disable cache"), WebInspector.settings.cacheDisabled));
    var disableJSElement = this._createCheckboxSetting(WebInspector.UIString("Disable JavaScript"), WebInspector.settings.javaScriptDisabled);
    p.appendChild(disableJSElement);
    WebInspector.settings.javaScriptDisabled.addChangeListener(this._javaScriptDisabledChanged, this);
    this._disableJSCheckbox = disableJSElement.getElementsByTagName("input")[0];
    this._updateScriptDisabledCheckbox();
    
    p = this._appendSection(WebInspector.UIString("Elements"));
    p.appendChild(this._createRadioSetting(WebInspector.UIString("Color format"), [
        [ WebInspector.StylesSidebarPane.ColorFormat.Original, WebInspector.UIString("As authored") ],
        [ WebInspector.StylesSidebarPane.ColorFormat.HEX, "HEX: #DAC0DE" ],
        [ WebInspector.StylesSidebarPane.ColorFormat.RGB, "RGB: rgb(128, 255, 255)" ],
        [ WebInspector.StylesSidebarPane.ColorFormat.HSL, "HSL: hsl(300, 80%, 90%)" ] ], WebInspector.settings.colorFormat));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show user agent styles"), WebInspector.settings.showUserAgentStyles));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Word wrap"), WebInspector.settings.domWordWrap));

    p = this._appendSection(WebInspector.UIString("Rendering"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show paint rectangles"), WebInspector.settings.showPaintRects));
    WebInspector.settings.showPaintRects.addChangeListener(this._showPaintRectsChanged, this);

    p = this._appendSection(WebInspector.UIString("Sources"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show folders"), WebInspector.settings.showScriptFolders));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Search in content scripts"), WebInspector.settings.searchInContentScripts));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Enable source maps"), WebInspector.settings.sourceMapsEnabled));
    p.appendChild(this._createSelectSetting(WebInspector.UIString("Indentation"), [
            [ WebInspector.UIString("2 spaces"), WebInspector.TextEditorModel.Indent.TwoSpaces ],
            [ WebInspector.UIString("4 spaces"), WebInspector.TextEditorModel.Indent.FourSpaces ],
            [ WebInspector.UIString("8 spaces"), WebInspector.TextEditorModel.Indent.EightSpaces ],
            [ WebInspector.UIString("Tab character"), WebInspector.TextEditorModel.Indent.TabCharacter ]
        ], WebInspector.settings.textEditorIndent));

    p = this._appendSection(WebInspector.UIString("Profiler"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Show objects' hidden properties"), WebInspector.settings.showHeapSnapshotObjectsHiddenProperties));

    p = this._appendSection(WebInspector.UIString("Console"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Log XMLHttpRequests"), WebInspector.settings.monitoringXHREnabled));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Preserve log upon navigation"), WebInspector.settings.preserveConsoleLog));

    if (WebInspector.extensionServer.hasExtensions()) {
        var handlerSelector = new WebInspector.HandlerSelector(WebInspector.openAnchorLocationRegistry);
        p = this._appendSection(WebInspector.UIString("Extensions"));
        p.appendChild(this._createCustomSetting(WebInspector.UIString("Open links in"), handlerSelector.element));
    }
}

WebInspector.GenericSettingsTab.prototype = {
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
    }
}

WebInspector.GenericSettingsTab.prototype.__proto__ = WebInspector.SettingsTab.prototype;

/**
 * @constructor
 * @extends {WebInspector.SettingsTab}
 */
WebInspector.UserAgentSettingsTab = function()
{
    WebInspector.SettingsTab.call(this);

    var p = this._appendSection();
    p.appendChild(this._createUserAgentControl());
    if (Capabilities.canOverrideDeviceMetrics)
        p.appendChild(this._createDeviceMetricsControl());
    if (Capabilities.canOverrideGeolocation && WebInspector.experimentsSettings.geolocationOverride.isEnabled())
        p.appendChild(this._createGeolocationOverrideControl());
    if (Capabilities.canOverrideDeviceOrientation && WebInspector.experimentsSettings.deviceOrientationOverride.isEnabled())
        p.appendChild(this._createDeviceOrientationOverrideControl());
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Emulate touch events"), WebInspector.settings.emulateTouchEvents));
}

WebInspector.UserAgentSettingsTab.prototype = {
    _createUserAgentControl: function()
    {
        var userAgent = WebInspector.settings.userAgent.get();

        var p = document.createElement("p");
        var labelElement = p.createChild("label");
        var checkboxElement = labelElement.createChild("input");
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !!userAgent;
        labelElement.appendChild(document.createTextNode("User Agent"));
        p.appendChild(this._createUserAgentSelectRowElement(checkboxElement));
        return p;
    },

    _createUserAgentSelectRowElement: function(checkboxElement)
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

            ["BlackBerry \u2014 PlayBook 2.1", "Mozilla/5.0 (PlayBook; U; RIM Tablet OS 2.1.0; en-US) AppleWebKit/536.2+ (KHTML, like Gecko) Version/7.2.1.0 Safari/536.2+", "1024x600x1"],
            ["BlackBerry \u2014 9900", "Mozilla/5.0 (BlackBerry; U; BlackBerry 9900; en-US) AppleWebKit/534.11+ (KHTML, like Gecko) Version/7.0.0.187 Mobile Safari/534.11+", "640x480x1"],
            ["BlackBerry \u2014 BB10", "Mozilla/5.0 (BB10; Touch) AppleWebKit/537.1+ (KHTML, like Gecko) Version/10.0.0.1337 Mobile Safari/537.1+", "768x1280x1"],

            ["MeeGo \u2014 Nokia N9", "Mozilla/5.0 (MeeGo; NokiaN9) AppleWebKit/534.13 (KHTML, like Gecko) NokiaBrowser/8.5.0 Mobile Safari/534.13", "480x854x1"],

            [WebInspector.UIString("Other..."), "Other"]
        ];

        var fieldsetElement = document.createElement("fieldset");
        this._selectElement = fieldsetElement.createChild("select");
        this._otherUserAgentElement = fieldsetElement.createChild("input");
        this._otherUserAgentElement.value = userAgent;
        this._otherUserAgentElement.title = userAgent;

        var selectionRestored = false;
        for (var i = 0; i < userAgents.length; ++i) {
            var agent = userAgents[i];
            var option = new Option(agent[0], agent[1]);
            option._metrics = agent[2] ? agent[2] : "";
            this._selectElement.add(option);
            if (userAgent === agent[1]) {
                this._selectElement.selectedIndex = i;
                selectionRestored = true;
            }
        }

        if (!selectionRestored) {
            if (!userAgent)
                this._selectElement.selectedIndex = 0;
            else
              this._selectElement.selectedIndex = userAgents.length - 1;
        }

        this._selectElement.addEventListener("change", this._selectionChanged.bind(this, true), false);

        fieldsetElement.addEventListener("dblclick", textDoubleClicked.bind(this), false);
        this._otherUserAgentElement.addEventListener("blur", textChanged.bind(this), false);

        function textDoubleClicked()
        {
            this._selectElement.selectedIndex = userAgents.length - 1;
            this._selectionChanged();
        }

        function textChanged()
        {
            WebInspector.settings.userAgent.set(this._otherUserAgentElement.value);
        }

        function checkboxClicked()
        {
            if (checkboxElement.checked) {
                this._selectElement.disabled = false;
                this._selectionChanged();
            } else {
                this._selectElement.disabled = true;
                this._otherUserAgentElement.disabled = true;
                WebInspector.settings.userAgent.set("");
            }
        }
        checkboxElement.addEventListener("click", checkboxClicked.bind(this), false);

        checkboxClicked.call(this);
        return fieldsetElement;
    },

    /**
     * @param {boolean=} isUserGesture
     */
    _selectionChanged: function(isUserGesture)
    {
        var value = this._selectElement.options[this._selectElement.selectedIndex].value;
        if (value !== "Other") {
            WebInspector.settings.userAgent.set(value);
            this._otherUserAgentElement.value = value;
            this._otherUserAgentElement.title = value;
            this._otherUserAgentElement.disabled = true;
        } else {
            this._otherUserAgentElement.disabled = false;
            this._otherUserAgentElement.focus();
        }

        if (isUserGesture && Capabilities.canOverrideDeviceMetrics) {
            var metrics = this._selectElement.options[this._selectElement.selectedIndex]._metrics;
            this._setDeviceMetricsOverride(WebInspector.UserAgentSupport.DeviceMetrics.parseSetting(metrics), false, true);
        }
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

    /**
     * Creates an input element under the parentElement with the given id and defaultText.
     * It also sets an onblur event listener.
     * @param {Element} parentElement
     * @param {string} id
     * @param {string} defaultText
     * @param {function(*)} eventListener
     * @return {Element} element
     */
    _createInput: function(parentElement, id, defaultText, eventListener)
    {
        var element = parentElement.createChild("input");
        element.id = id;
        element.maxLength = 12;
        element.style.width = "80px";
        element.value = defaultText;
        element.align = "right";
        element.addEventListener("blur", eventListener, false);
        return element;
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
        labelElement.appendChild(document.createTextNode(WebInspector.UIString("Device metrics")));

        const metricsSectionElement = this._createDeviceMetricsElement(metrics);
        p.appendChild(metricsSectionElement);
        this._metricsSectionElement = metricsSectionElement;

        this._setDeviceMetricsOverride(metrics, false, true);

        return p;
    },

    _onMetricsCheckboxClicked: function()
    {
        var controlsDisabled = !this._metricsCheckboxElement.checked;
        this._widthOverrideElement.disabled = controlsDisabled;
        this._heightOverrideElement.disabled = controlsDisabled;
        this._fontScaleFactorOverrideElement.disabled = controlsDisabled;
        this._swapDimensionsElement.disabled = controlsDisabled;
        this._fitWindowCheckboxElement.disabled = controlsDisabled;

        if (this._metricsCheckboxElement.checked) {
            var metrics = WebInspector.UserAgentSupport.DeviceMetrics.parseUserInput(this._widthOverrideElement.value, this._heightOverrideElement.value, this._fontScaleFactorOverrideElement.value);
            if (metrics && metrics.isValid() && metrics.width && metrics.height)
                this._setDeviceMetricsOverride(metrics, false, false);
            if (!this._widthOverrideElement.value)
                this._widthOverrideElement.focus();
        } else {
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

        function swapDimensionsClicked(event)
        {
            var widthValue = this._widthOverrideElement.value;
            this._widthOverrideElement.value = this._heightOverrideElement.value;
            this._heightOverrideElement.value = widthValue;
            this._applyDeviceMetricsUserInput();
        }

        var tableElement = fieldsetElement.createChild("table", "nowrap");

        var rowElement = tableElement.createChild("tr");
        var cellElement = rowElement.createChild("td");
        cellElement.appendChild(document.createTextNode(WebInspector.UIString("Screen resolution:")));
        cellElement = rowElement.createChild("td");
        this._widthOverrideElement = this._createInput(cellElement, "metrics-override-width", String(metrics.width || screen.width), this._applyDeviceMetricsUserInput.bind(this));
        cellElement.appendChild(document.createTextNode(" \u00D7 ")); // MULTIPLICATION SIGN.
        this._heightOverrideElement = this._createInput(cellElement, "metrics-override-height", String(metrics.height || screen.height), this._applyDeviceMetricsUserInput.bind(this));
        cellElement.appendChild(document.createTextNode(" \u2014 ")); // EM DASH.
        this._swapDimensionsElement = cellElement.createChild("button");
        this._swapDimensionsElement.appendChild(document.createTextNode(" \u21C4 ")); // RIGHTWARDS ARROW OVER LEFTWARDS ARROW.
        this._swapDimensionsElement.title = WebInspector.UIString("Swap dimensions");
        this._swapDimensionsElement.addEventListener("click", swapDimensionsClicked.bind(this), false);

        rowElement = tableElement.createChild("tr");
        cellElement = rowElement.createChild("td");
        cellElement.appendChild(document.createTextNode(WebInspector.UIString("Font scale factor:")));
        cellElement = rowElement.createChild("td");
        this._fontScaleFactorOverrideElement = this._createInput(cellElement, "metrics-override-font-scale", String(metrics.fontScaleFactor || 1), this._applyDeviceMetricsUserInput.bind(this));

        rowElement = tableElement.createChild("tr");
        cellElement = rowElement.createChild("td");
        cellElement.colspan = 2;
        this._fitWindowCheckboxElement = document.createElement("input");
        cellElement.appendChild(this._createCheckboxSetting(WebInspector.UIString("Fit in window"), WebInspector.settings.deviceFitWindow, true, this._fitWindowCheckboxElement));

        return fieldsetElement;
    },

    _createGeolocationOverrideControl: function()
    {
        const geolocationSetting = WebInspector.settings.geolocationOverride.get();
        var geolocation = WebInspector.UserAgentSupport.GeolocationPosition.parseSetting(geolocationSetting);
        var p = document.createElement("p");
        var labelElement = p.createChild("label");
        var checkboxElement = labelElement.createChild("input");
        checkboxElement.id = "geolocation-override-checkbox";
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !geolocation || (((typeof geolocation.latitude === "number") && (typeof geolocation.longitude === "number")) || geolocation.error);
        checkboxElement.addEventListener("click", this._onGeolocationOverrideCheckboxClicked.bind(this), false);
        this._geolocationOverrideCheckboxElement = checkboxElement;
        labelElement.appendChild(document.createTextNode(WebInspector.UIString("Override Geolocation")));

        var geolocationSectionElement = this._createGeolocationOverrideElement(geolocation);
        p.appendChild(geolocationSectionElement);
        this._geolocationSectionElement = geolocationSectionElement;
        this._setGeolocationPosition(geolocation, false, true);
        return p;
    },

    _onGeolocationOverrideCheckboxClicked: function()
    {
        var controlsDisabled = !this._geolocationOverrideCheckboxElement.checked;
        this._latitudeElement.disabled = controlsDisabled;
        this._longitudeElement.disabled = controlsDisabled;
        this._geolocationErrorElement.disabled = controlsDisabled;

        if (this._geolocationOverrideCheckboxElement.checked) {
            var geolocation = WebInspector.UserAgentSupport.GeolocationPosition.parseUserInput(this._latitudeElement.value, this._longitudeElement.value, this._geolocationErrorElement.checked);
            if (geolocation)
                this._setGeolocationPosition(geolocation, false, false);
            if (!this._latitudeElement.value)
                this._latitudeElement.focus();
        } else
            WebInspector.UserAgentSupport.GeolocationPosition.clearGeolocationOverride();
    },

    _applyGeolocationUserInput: function()
    {
        this._setGeolocationPosition(WebInspector.UserAgentSupport.GeolocationPosition.parseUserInput(this._latitudeElement.value.trim(), this._longitudeElement.value.trim(), this._geolocationErrorElement.checked), true, false);
    },

    /**
     * @param {?WebInspector.UserAgentSupport.GeolocationPosition} geolocation
     * @param {boolean} userInputModified
     * @param {boolean} updateCheckbox
     */
    _setGeolocationPosition: function(geolocation, userInputModified, updateCheckbox)
    {
        if (!geolocation)
            return;

        if (!userInputModified) {
            this._latitudeElement.value = geolocation.latitude;
            this._longitudeElement.value = geolocation.longitude;
        }

        var value = geolocation.toSetting();
        WebInspector.settings.geolocationOverride.set(value);

        if (this._geolocationOverrideCheckboxElement && updateCheckbox) {
            this._geolocationOverrideCheckboxElement.checked = !!geolocation.toSetting();
            this._onGeolocationOverrideCheckboxClicked();
        }
    },

    /**
     * @param {WebInspector.UserAgentSupport.GeolocationPosition} geolocation
     */
    _createGeolocationOverrideElement: function(geolocation)
    {
        var fieldsetElement = document.createElement("fieldset");
        fieldsetElement.id = "geolocation-override-section";

        var tableElement = fieldsetElement.createChild("table");
        var rowElement = tableElement.createChild("tr");
        var cellElement = rowElement.createChild("td");
        cellElement.appendChild(document.createTextNode(WebInspector.UIString("Geolocation Position") + ":"));
        cellElement = rowElement.createChild("td");
        this._latitudeElement = this._createInput(cellElement, "geolocation-override-latitude", String(geolocation.latitude), this._applyGeolocationUserInput.bind(this));
        cellElement.appendChild(document.createTextNode(" , "));
        this._longitudeElement = this._createInput(cellElement, "geolocation-override-longitude", String(geolocation.longitude), this._applyGeolocationUserInput.bind(this));
        rowElement = tableElement.createChild("tr");
        cellElement = rowElement.createChild("td");
        var geolocationErrorLabelElement = document.createElement("label");
        var geolocationErrorCheckboxElement = geolocationErrorLabelElement.createChild("input");
        geolocationErrorCheckboxElement.id = "geolocation-error";
        geolocationErrorCheckboxElement.type = "checkbox";
        geolocationErrorCheckboxElement.checked = !geolocation || geolocation.error;
        geolocationErrorCheckboxElement.addEventListener("click", this._applyGeolocationUserInput.bind(this), false);
        geolocationErrorLabelElement.appendChild(document.createTextNode(WebInspector.UIString("Emulate position unavailable")));
        this._geolocationErrorElement = geolocationErrorCheckboxElement;
        cellElement.appendChild(geolocationErrorLabelElement);

        return fieldsetElement;
    },

    _createDeviceOrientationOverrideControl: function()
    {
        const deviceOrientationSetting = WebInspector.settings.deviceOrientationOverride.get();
        var deviceOrientation = WebInspector.UserAgentSupport.DeviceOrientation.parseSetting(deviceOrientationSetting);

        var p = document.createElement("p");
        var labelElement = p.createChild("label");
        var checkboxElement = labelElement.createChild("input");
        checkboxElement.id = "device-orientation-override-checkbox";
        checkboxElement.type = "checkbox";
        checkboxElement.checked = !deviceOrientation;
        checkboxElement.addEventListener("click", this._onDeviceOrientationOverrideCheckboxClicked.bind(this), false);
        this._deviceOrientationOverrideCheckboxElement = checkboxElement;
        labelElement.appendChild(document.createTextNode(WebInspector.UIString("Override Device Orientation")));

        var deviceOrientationSectionElement = this._createDeviceOrientationOverrideElement(deviceOrientation);
        p.appendChild(deviceOrientationSectionElement);
        this._deviceOrientationSectionElement = deviceOrientationSectionElement;

        this._setDeviceOrientation(deviceOrientation, false, true);

        return p;
    },

    _onDeviceOrientationOverrideCheckboxClicked: function()
    {
        var controlsDisabled = !this._deviceOrientationOverrideCheckboxElement.checked;
        this._alphaElement.disabled = controlsDisabled;
        this._betaElement.disabled = controlsDisabled;
        this._gammaElement.disabled = controlsDisabled;

        if (this._deviceOrientationOverrideCheckboxElement.checked) {
            var deviceOrientation = WebInspector.UserAgentSupport.DeviceOrientation.parseUserInput(this._alphaElement.value, this._betaElement.value, this._gammaElement.value);
            if (deviceOrientation)
                this._setDeviceOrientation(deviceOrientation, false, false);
            if (!this._alphaElement.value)
                this._alphaElement.focus();
        } else
            WebInspector.UserAgentSupport.DeviceOrientation.clearDeviceOrientationOverride();
    },

    _applyDeviceOrientationUserInput: function()
    {
        this._setDeviceOrientation(WebInspector.UserAgentSupport.DeviceOrientation.parseUserInput(this._alphaElement.value.trim(), this._betaElement.value.trim(), this._gammaElement.value.trim()), true, false);
    },

    /**
     * @param {?WebInspector.UserAgentSupport.DeviceOrientation} deviceOrientation
     * @param {boolean} userInputModified
     * @param {boolean} updateCheckbox
     */
    _setDeviceOrientation: function(deviceOrientation, userInputModified, updateCheckbox)
    {
        if (!deviceOrientation)
            return;

        if (!userInputModified) {
            this._alphaElement.value = deviceOrientation.alpha;
            this._betaElement.value = deviceOrientation.beta;
            this._gammaElement.value = deviceOrientation.gamma;
        }

        var value = deviceOrientation.toSetting();
        WebInspector.settings.deviceOrientationOverride.set(value);

        if (this._deviceOrientationOverrideCheckboxElement && updateCheckbox) {
            this._deviceOrientationOverrideCheckboxElement.checked = !!deviceOrientation.toSetting();
            this._onDeviceOrientationOverrideCheckboxClicked();
        }
    },

    /**
     * @param {WebInspector.UserAgentSupport.DeviceOrientation} deviceOrientation
     */
    _createDeviceOrientationOverrideElement: function(deviceOrientation)
    {
        var fieldsetElement = document.createElement("fieldset");
        fieldsetElement.id = "device-orientation-override-section";

        var tableElement = fieldsetElement.createChild("table");

        var rowElement = tableElement.createChild("tr");
        var cellElement = rowElement.createChild("td");
        cellElement.appendChild(document.createTextNode("\u03B1: "));
        this._alphaElement = this._createInput(cellElement, "device-orientation-override-alpha", String(deviceOrientation.alpha), this._applyDeviceOrientationUserInput.bind(this));
        cellElement.appendChild(document.createTextNode(" \u03B2: "));
        this._betaElement = this._createInput(cellElement, "device-orientation-override-beta", String(deviceOrientation.beta), this._applyDeviceOrientationUserInput.bind(this));
        cellElement.appendChild(document.createTextNode(" \u03B3: "));
        this._gammaElement = this._createInput(cellElement, "device-orientation-override-gamma", String(deviceOrientation.gamma), this._applyDeviceOrientationUserInput.bind(this));

        return fieldsetElement;
    }
}

WebInspector.UserAgentSettingsTab.prototype.__proto__ = WebInspector.SettingsTab.prototype;

/**
 * @constructor
 * @extends {WebInspector.SettingsTab}
 */
WebInspector.ExperimentsSettingsTab = function()
{
    WebInspector.SettingsTab.call(this);

    var experiments = WebInspector.experimentsSettings.experiments;
    if (experiments.length) {
        var experimentsSection = this._appendSection();
        experimentsSection.appendChild(this._createExperimentsWarningSubsection());
        for (var i = 0; i < experiments.length; ++i)
            experimentsSection.appendChild(this._createExperimentCheckbox(experiments[i]));
    }
}

WebInspector.ExperimentsSettingsTab.prototype = {
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
    }
}

WebInspector.ExperimentsSettingsTab.prototype.__proto__ = WebInspector.SettingsTab.prototype;

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
            this.showSettingsScreen();
    },

    _onHideSettingsScreen: function()
    {
        this._statusBarButton.toggled = false;
    },

    /**
     * @param {string=} tabId
     */
    showSettingsScreen: function(tabId)
    {
        if (!this._settingsScreen)
            this._settingsScreen = new WebInspector.SettingsScreen(this._onHideSettingsScreen.bind(this));

        if (tabId)
            this._settingsScreen.selectTab(tabId);

        this._settingsScreen.showModal();
        this._statusBarButton.toggled = true;
    },

    _hideSettingsScreen: function()
    {
        if (this._settingsScreen)
            this._settingsScreen.hide();
    },

    resize: function()
    {
        if (this._settingsScreen && this._settingsScreen.isShowing())
            this._settingsScreen.doResize();
    }
}
