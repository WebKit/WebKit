/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
WebInspector.ShortcutsScreen = function()
{
    this._sections = {};
}

WebInspector.ShortcutsScreen.prototype = {
    section: function(name)
    {
        var section = this._sections[name];
        if (!section)
            this._sections[name] = section = new WebInspector.ShortcutsSection(name);
        return section;
    },

    _createShortcutsTabView: function()
    {
        var orderedSections = [];
        for (var section in this._sections)
            orderedSections.push(this._sections[section]);
        function compareSections(a, b)
        {
            return a.order - b.order;
        }
        orderedSections.sort(compareSections);

        var view = new WebInspector.View();

        var container = view.element;
        container.className = "help-content help-container";
        for (var i = 0; i < orderedSections.length; ++i)
            orderedSections[i].renderSection(container);

        return view;
    }
}

/**
 * We cannot initialize it here as localized strings are not loaded yet.
 * @type {?WebInspector.ShortcutsScreen}
 */
WebInspector.shortcutsScreen = null;

/**
 * @constructor
 */
WebInspector.ShortcutsSection = function(name)
{
    this.name = name;
    this._lines = [];
    this.order = ++WebInspector.ShortcutsSection._sequenceNumber;
};

WebInspector.ShortcutsSection._sequenceNumber = 0;

WebInspector.ShortcutsSection.prototype = {
    addKey: function(key, description)
    {
        this._addLine(this._renderKey(key), description);
    },

    addRelatedKeys: function(keys, description)
    {
        this._addLine(this._renderSequence(keys, "/"), description);
    },

    addAlternateKeys: function(keys, description)
    {
        this._addLine(this._renderSequence(keys, WebInspector.UIString("or")), description);
    },

    _addLine: function(keyElement, description)
    {
        this._lines.push({ key: keyElement, text: description })
    },

    /**
     * @param {!Element} container
     */
    renderSection: function(container)
    {
        var parent = container.createChild("div", "help-block");

        var headLine = parent.createChild("div", "help-line");
        headLine.createChild("div", "help-key-cell");
        headLine.createChild("div", "help-section-title help-cell").textContent = this.name;

        for (var i = 0; i < this._lines.length; ++i) {
            var line = parent.createChild("div", "help-line");
            var keyCell = line.createChild("div", "help-key-cell");
            keyCell.appendChild(this._lines[i].key);
            keyCell.appendChild(this._createSpan("help-key-delimiter", ":"));
            line.createChild("div", "help-cell").textContent = this._lines[i].text;
        }
    },

    _renderSequence: function(sequence, delimiter)
    {
        var delimiterSpan = this._createSpan("help-key-delimiter", delimiter);
        return this._joinNodes(sequence.map(this._renderKey.bind(this)), delimiterSpan);
    },

    _renderKey: function(key)
    {
        var plus = this._createSpan("help-combine-keys", "+");
        return this._joinNodes(key.split(" + ").map(this._createSpan.bind(this, "help-key monospace")), plus);
    },

    _createSpan: function(className, textContent)
    {
        var node = document.createElement("span");
        node.className = className;
        node.textContent = textContent;
        return node;
    },

    _joinNodes: function(nodes, delimiter)
    {
        var result = document.createDocumentFragment();
        for (var i = 0; i < nodes.length; ++i) {
            if (i > 0)
                result.appendChild(delimiter.cloneNode(true));
            result.appendChild(nodes[i]);
        }
        return result;
    }
}
