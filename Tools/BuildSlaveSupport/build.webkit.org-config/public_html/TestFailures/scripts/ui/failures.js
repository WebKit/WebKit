/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

var ui = ui || {};
ui.failures = ui.failures || {};

(function(){

var kBuildingResult = 'BUILDING';

ui.failures.Configuration = base.extends('a', {
    init: function(configuration)
    {
        if (configuration.is64bit)
            this._addSpan('architecture', '64-bit');
        if (configuration.version)
            this._addSpan('version', configuration.version);
        this._configuration = configuration;
        this.target = '_blank';
    },
    _addSpan: function(className, text)
    {
        var span = this.appendChild(document.createElement('span'));
        span.className = className;
        span.textContent = text;
    },
    equals: function(configuration)
    {
        return this._configuration.is64bit == configuration.is64bit && this._configuration.version == configuration.version; 
    }
});

function cellContainsConfiguration(cell, configuration)
{
    return Array.prototype.some.call(cell.children, function(configurationElement) {
        return configurationElement.equals && configurationElement.equals(configuration);
    });
}

function cellByBuildType(row, configuration)
{
    return row.cells[configuration.debug ? 2 : 1];
}

ui.failures.FailureGrid = base.extends('table', {
    init: function()
    {
        this.className = 'failures';
        var titles = this.createTHead().insertRow();
        titles.insertCell().textContent = 'debug';
        titles.insertCell().textContent = 'release';
        titles.insertCell().textContent = 'type';
        this._body = this.appendChild(document.createElement('tbody'));
        this._resultRows = {};
        // Add the BUILDING row eagerly so that it appears last.
        this._rowByResult(kBuildingResult);
        $(this._resultRows[kBuildingResult]).hide();
    },
    _rowByResult: function(result)
    {
        var row = this._resultRows[result];
        $(row).show();
        if (row)
            return row;

        row = this._resultRows[result] = this._body.insertRow(0);
        row.className = result;
        row.insertCell();
        row.insertCell();
        row.insertCell().textContent = result;
        return row;
    },
    removeResultRows: function()
    {
        Object.keys(this._resultRows).forEach(function(result) {
            var row = this._resultRows[result];
            row.parentNode.removeChild(row);
        }, this);
        this._resultRows = {};
    },
    add: function(resultsByBuilder)
    {
        Object.keys(resultsByBuilder).forEach(function(builderName) {
            var configuration = config.kBuilders[builderName];
            var row = this._rowByResult(resultsByBuilder[builderName].actual);
            var cell = cellByBuildType(row, configuration);
            if (cellContainsConfiguration(cell, configuration))
                return;
            cell.appendChild(new ui.failures.Configuration(configuration)).href = ui.displayURLForBuilder(builderName);
        }, this);
    }
});

})();
