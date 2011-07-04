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

WebInspector.SettingsScreen = function()
{
    WebInspector.HelpScreen.call(this, WebInspector.UIString("Settings"));

    this._leftColumnElement = document.createElement("td");
    this._rightColumnElement = document.createElement("td");

    var p = this._appendSection("Console");
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Log XMLHttpRequests"), "monitoringXHREnabled"));
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Preserve log upon navigation"), "preserveConsoleLog"));

    p = this._appendSection("Elements");
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Word wrap"), "domWordWrap"));
    p.appendChild(this._createRadioSetting(WebInspector.UIString("Color format"), [WebInspector.UIString("As authored"), "hex", "rgb", "hsl"]));

    p = this._appendSection("Network", true);
    p.appendChild(this._createCheckboxSetting(("Enable background events collection")));

    p = this._appendSection("Scripts", true);
    p.appendChild(this._createCheckboxSetting(WebInspector.UIString("Enable workers debugging")));

    var table = document.createElement("table");
    table.className = "help-table";
    var tr = document.createElement("tr");
    tr.appendChild(this._leftColumnElement);
    tr.appendChild(this._rightColumnElement);
    table.appendChild(tr);
    this.contentElement.appendChild(table);
}

WebInspector.SettingsScreen.prototype = {
    _appendSection: function(name, right)
    {
        var p = document.createElement("p");
        p.className = "help-section";
        var title = document.createElement("div");
        title.className = "help-section-title";
        title.textContent = name;
        p.appendChild(title);
        this._columnElement(right).appendChild(p);
        return p;
    },

    _columnElement: function(right)
    {
        return right ? this._rightColumnElement : this._leftColumnElement; 
    },

    _createCheckboxSetting: function(name, settingName)
    {
        var input = document.createElement("input");
        input.type = "checkbox";
        input.name = name;
        if (settingName) {
            input.checked = WebInspector.settings[settingName].get();
            function listener()
            {
                WebInspector.settings[settingName].set(input.checked);
            }
            input.addEventListener("click", listener, false);
        }

        var p = document.createElement("p");
        var label = document.createElement("label");
        label.appendChild(input);
        label.appendChild(document.createTextNode(name));
        p.appendChild(label);
        return p;
    },

    _createRadioSetting: function(name, options)
    {
        var pp = document.createElement("p");
        var fieldsetElement = document.createElement("fieldset");
        var legendElement = document.createElement("legend");
        legendElement.textContent = name;
        fieldsetElement.appendChild(legendElement);

        for (var i = 0; i < options.length; ++i) {
          var p = document.createElement("p");
          var label = document.createElement("label");
          p.appendChild(label);

          var input = document.createElement("input");
          input.type = "radio";
          input.name = name;
          label.appendChild(input);
          label.appendChild(document.createTextNode(options[i]));

          fieldsetElement.appendChild(p);
        }

        pp.appendChild(fieldsetElement);
        return pp;
    }
};

WebInspector.SettingsScreen.prototype.__proto__ = WebInspector.HelpScreen.prototype;
