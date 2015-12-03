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

WebInspector.VisualStylePropertyCombiner = class VisualStylePropertyCombiner extends WebInspector.Object
{
    constructor(propertyName, propertyEditors)
    {
        super();

        this._style = null;
        this._propertyName = propertyName;
        this._propertyMissing = false;
        this._propertyEditors = propertyEditors || [];

        for (var editor of this._propertyEditors) {
            editor.addEventListener(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange, this._handlePropertyEditorValueChanged, this);
            editor.suppressStyleTextUpdate = true;
        }

        this._textContainsNameRegExp = new RegExp("(?:(?:^|;)\\s*" + this._propertyName + "\\s*:)");
        this._replacementRegExp = new RegExp("((?:^|;)\\s*)(" + this._propertyName + ")(.+?(?:;|$))");
        this._valueRegExp = /([^\s]+\(.+\)|[^\s]+)(?:;?)/g;
    }

    get style()
    {
        return this._style;
    }

    get synthesizedValue()
    {
        var value = "";
        var oneEditorHasValue = false;
        for (var editor of this._propertyEditors) {
            var editorValue = editor.synthesizedValue;
            if (editorValue && editorValue.length)
                oneEditorHasValue = true;
            else if (editor.optionalProperty)
                continue;

            if (editor.masterProperty && editor.valueIsSupportedKeyword()) {
                this._markEditors(editor, true);
                return editorValue;
            }

            if (editor !== this._propertyEditors[0])
                value += " ";

            value += editorValue || (editor.colorProperty ? "transparent" : 0);
        }

        this._markEditors();
        return value.length && oneEditorHasValue ? value : null;
    }

    modifyPropertyText(text, value)
    {
        if (this._textContainsNameRegExp.test(text))
            text = text.replace(this._replacementRegExp, value !== null ? "$1$2: " + value + ";" : "$1");
        else if (value !== null)
            text += WebInspector.VisualStylePropertyEditor.generateFormattedTextForNewProperty(text, this._propertyName, value);

        return text;
    }

    update(style)
    {
        if (style)
            this._style = style;
        else if (this._ignoreNextUpdate) {
            this._ignoreNextUpdate = false;
            return;
        }

        if (!this._style || !this._valueRegExp)
            return;

        var property = this._style.propertyForName(this._propertyName, true);
        var propertyMissing = !property;
        if (propertyMissing && this._style.nodeStyles)
            property = this._style.nodeStyles.computedStyle.propertyForName(this._propertyName);

        if (!property)
            return;

        this.updateValuesFromText(property.value, propertyMissing);
        this._propertyMissing = propertyMissing;
    }

    updateValuesFromText(styleText, propertyMissing)
    {
        if (styleText === this.synthesizedValue)
            return;

        for (var editor of this._propertyEditors)
            editor[WebInspector.VisualStylePropertyCombiner.EditorUpdatedSymbol] = false;

        function updateCompatibleEditor(value) {
            for (var editor of this._propertyEditors) {
                if (value && !editor.valueIsCompatible(value) || editor[WebInspector.VisualStylePropertyCombiner.EditorUpdatedSymbol])
                    continue;

                if (this._currentValueIsKeyword && editor.disabled)
                    continue;

                var updatedValues = editor.getValuesFromText(value || "", propertyMissing);
                if (updatedValues)
                    editor.updateEditorValues(updatedValues);
                editor[WebInspector.VisualStylePropertyCombiner.EditorUpdatedSymbol] = true;

                if (value)
                    return;
            }
        }

        var matches = styleText.match(this._valueRegExp);
        for (var i = 0; i < this._propertyEditors.length; ++i)
            updateCompatibleEditor.call(this, matches && matches[i]);
    }

    get propertyMissing()
    {
        return this._propertyMissing;
    }

    resetEditorValues(value)
    {
        this._ignoreNextUpdate = false;
        this.updateValuesFromText(value || "");
    }

    // Private

    _markEditors(ignoredEditor, disabled)
    {
        this._currentValueIsKeyword = disabled || false;
        for (var editor of this._propertyEditors) {
            if (ignoredEditor && editor === ignoredEditor)
                continue;

            editor.disabled = this._currentValueIsKeyword;
        }
    }

    _handlePropertyEditorValueChanged()
    {
        this._ignoreNextUpdate = true;
        var value = this.synthesizedValue;
        if (this._style)
            this._style.text = this.modifyPropertyText(this._style.text, value);

        this._propertyMissing = !value;
        this.dispatchEventToListeners(WebInspector.VisualStylePropertyEditor.Event.ValueDidChange);
    }
};

WebInspector.VisualStylePropertyCombiner.EditorUpdatedSymbol = Symbol("visual-style-property-combiner-editor-updated");
