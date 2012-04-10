/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 */
WebInspector.UserAgentSupport = function()
{
    if (WebInspector.settings.deviceMetrics.get())
        this._deviceMetricsChanged();
    WebInspector.settings.deviceMetrics.addChangeListener(this._deviceMetricsChanged, this);
    WebInspector.settings.deviceFitWindow.addChangeListener(this._deviceMetricsChanged, this);
}

/**
 * @constructor
 * @param {number} width
 * @param {number} height
 * @param {number} fontScaleFactor
 */
WebInspector.UserAgentSupport.DeviceMetrics = function(width, height, fontScaleFactor)
{
    this.width = width;
    this.height = height;
    this.fontScaleFactor = fontScaleFactor;
}

/**
 * @return {WebInspector.UserAgentSupport.DeviceMetrics}
 */
WebInspector.UserAgentSupport.DeviceMetrics.parseSetting = function(value)
{
    if (value) {
        var splitMetrics = value.split("x");
        if (splitMetrics.length === 3)
            return new WebInspector.UserAgentSupport.DeviceMetrics(parseInt(splitMetrics[0], 10), parseInt(splitMetrics[1], 10), parseFloat(splitMetrics[2]));
    }
    return new WebInspector.UserAgentSupport.DeviceMetrics(0, 0, 1);
}

/**
 * @return {?WebInspector.UserAgentSupport.DeviceMetrics}
 */
WebInspector.UserAgentSupport.DeviceMetrics.parseUserInput = function(widthString, heightString, fontScaleFactorString)
{
    function isUserInputValid(value, isInteger)
    {
        if (!value)
            return true;
        return isInteger ? /^[0]*[1-9][\d]*$/.test(value) : /^[0]*([1-9][\d]*(\.\d+)?|\.\d+)$/.test(value);
    }

    if (!widthString ^ !heightString)
        return null;

    var isWidthValid = isUserInputValid(widthString, true);
    var isHeightValid = isUserInputValid(heightString, true);
    var isFontScaleFactorValid = isUserInputValid(fontScaleFactorString, false);

    if (!isWidthValid && !isHeightValid && !isFontScaleFactorValid)
        return null;

    var width = isWidthValid ? parseInt(widthString || "0", 10) : -1;
    var height = isHeightValid ? parseInt(heightString || "0", 10) : -1;
    var fontScaleFactor = isFontScaleFactorValid ? parseFloat(fontScaleFactorString) : -1;

    return new WebInspector.UserAgentSupport.DeviceMetrics(width, height, fontScaleFactor);
}

WebInspector.UserAgentSupport.DeviceMetrics.prototype = {
    /**
     * @return {boolean}
     */
    isValid: function()
    {
        return this.isWidthValid() && this.isHeightValid() && this.isFontScaleFactorValid();
    },

    /**
     * @return {boolean}
     */
    isWidthValid: function()
    {
        return this.width >= 0;
    },

    /**
     * @return {boolean}
     */
    isHeightValid: function()
    {
        return this.height >= 0;
    },

    /**
     * @return {boolean}
     */
    isFontScaleFactorValid: function()
    {
        return this.fontScaleFactor > 0;
    },

    /**
     * @return {string}
     */
    toSetting: function()
    {
        if (!this.isValid())
            return "";

        return this.width && this.height ? this.width + "x" + this.height + "x" + this.fontScaleFactor : "";
    },

    /**
     * @return {string}
     */
    widthToInput: function()
    {
        return this.isWidthValid() && this.width ? String(this.width) : "";
    },

    /**
     * @return {string}
     */
    heightToInput: function()
    {
        return this.isHeightValid() && this.height ? String(this.height) : "";
    },

    /**
     * @return {string}
     */
    fontScaleFactorToInput: function()
    {
        return this.isFontScaleFactorValid() && this.fontScaleFactor ? String(this.fontScaleFactor) : "";
    }
}

WebInspector.UserAgentSupport.prototype = {
    _deviceMetricsChanged: function()
    {
        var metrics = WebInspector.UserAgentSupport.DeviceMetrics.parseSetting(WebInspector.settings.deviceMetrics.get());
        if (metrics.isValid())
            PageAgent.setDeviceMetricsOverride(metrics.width, metrics.height, metrics.fontScaleFactor, WebInspector.settings.deviceFitWindow.get());
    }
}