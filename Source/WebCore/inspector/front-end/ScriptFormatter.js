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

WebInspector.ScriptFormatter = function()
{
    this._worker = new Worker("ScriptFormatterWorker.js");
    this._worker.onmessage = this._handleMessage.bind(this);
    this._worker.onerror = this._handleError.bind(this);
    this._tasks = [];
}

WebInspector.ScriptFormatter.locationToPosition = function(lineEndings, location)
{
    var position = location.lineNumber ? lineEndings[location.lineNumber - 1] + 1 : 0;
    return position + location.columnNumber;
}

WebInspector.ScriptFormatter.lineToPosition = function(lineEndings, lineNumber)
{
    return this.locationToPosition(lineEndings, { lineNumber: lineNumber, columnNumber: 0 });
}

WebInspector.ScriptFormatter.positionToLocation = function(lineEndings, position)
{
    var location = {};
    location.lineNumber = lineEndings.upperBound(position - 1);
    if (!location.lineNumber)
        location.columnNumber = position;
    else
        location.columnNumber = position - lineEndings[location.lineNumber - 1] - 1;
    return location;
}

WebInspector.ScriptFormatter.findScriptRanges = function(lineEndings, scripts)
{
    var scriptRanges = [];
    for (var i = 0; i < scripts.length; ++i) {
        var start = { lineNumber: scripts[i].lineOffset, columnNumber: scripts[i].columnOffset };
        start.position = WebInspector.ScriptFormatter.locationToPosition(lineEndings, start);
        var endPosition = start.position + scripts[i].length;
        var end = WebInspector.ScriptFormatter.positionToLocation(lineEndings, endPosition);
        end.position = endPosition;
        scriptRanges.push({ start: start, end: end, sourceID: scripts[i].sourceID });
    }
    scriptRanges.sort(function(x, y) { return x.start.position - y.start.position; });
    return scriptRanges;
}

WebInspector.ScriptFormatter.prototype = {
    formatContent: function(text, scripts, callback)
    {
        var scriptRanges = WebInspector.ScriptFormatter.findScriptRanges(text.lineEndings(), scripts);
        var chunks = this._splitContentIntoChunks(text, scriptRanges);

        function didFormatChunks()
        {
            var result = this._buildContentFromChunks(chunks);
            callback(result.text, result.mapping);
        }
        this._formatChunks(chunks, 0, didFormatChunks.bind(this));
    },

    _splitContentIntoChunks: function(text, scriptRanges)
    {
        var chunks = [];
        function addChunk(start, end, isScript)
        {
            var chunk = {};
            chunk.start = start;
            chunk.end = end;
            chunk.isScript = isScript;
            chunk.text = text.substring(start, end);
            chunks.push(chunk);
        }
        var currentPosition = 0;
        for (var i = 0; i < scriptRanges.length; ++i) {
            var start = scriptRanges[i].start.position;
            var end = scriptRanges[i].end.position;
            if (currentPosition < start)
                addChunk(currentPosition, start, false);
            addChunk(start, end, true);
            currentPosition = end;
        }
        if (currentPosition < text.length)
            addChunk(currentPosition, text.length, false);
        return chunks;
    },

    _formatChunks: function(chunks, index, callback)
    {
        while(true) {
            if (index === chunks.length) {
                callback();
                return;
            }
            var chunk = chunks[index++];
            if (chunk.isScript)
                break;
        }

        function didFormat(formattedSource, mapping)
        {
            chunk.text = formattedSource;
            chunk.mapping = mapping;
            this._formatChunks(chunks, index, callback);
        }
        this._formatScript(chunk.text, didFormat.bind(this));
    },

    _buildContentFromChunks: function(chunks)
    {
        var text = "";
        var mapping = { original: [], formatted: [] };
        for (var i = 0; i < chunks.length; ++i) {
            var chunk = chunks[i];
            mapping.original.push(chunk.start);
            mapping.formatted.push(text.length);
            if (chunk.isScript) {
                if (text)
                    text += "\n";
                for (var j = 0; j < chunk.mapping.original.length; ++j) {
                    mapping.original.push(chunk.mapping.original[j] + chunk.start);
                    mapping.formatted.push(chunk.mapping.formatted[j] + text.length);
                }
                text += chunk.text;
            } else {
                if (text)
                    text += "\n";
                text += chunk.text;
            }
            mapping.original.push(chunk.end);
            mapping.formatted.push(text.length);
        }
        return { text: text, mapping: mapping };
    },

    _formatScript: function(source, callback)
    {
        this._tasks.push({ source: source, callback: callback });
        this._worker.postMessage(source);
    },

    _handleMessage: function(event)
    {
        var task = this._tasks.shift();
        task.callback(event.data.formattedSource, event.data.mapping);
    },

    _handleError: function(event)
    {
        console.warn("Error in script formatter worker:", event);
        event.preventDefault()
        var task = this._tasks.shift();
        task.callback(task.source, { original: [], formatted: [] });
    }
}
