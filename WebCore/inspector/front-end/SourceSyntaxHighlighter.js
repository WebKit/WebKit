/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Joseph Pecoraro
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.SourceSyntaxHighlighter = function(table, sourceFrame)
{
    this.table = table;
    this.sourceFrame = sourceFrame;
}

WebInspector.SourceSyntaxHighlighter.prototype = {
    createSpan: function(content, className)
    {
        var span = document.createElement("span");
        span.className = className;
        span.appendChild(document.createTextNode(content));
        return span;
    },

    process: function()
    {
        // Split up the work into chunks so we don't block the
        // UI thread while processing.

        var rows = this.table.rows;
        var rowsLength = rows.length;
        const tokensPerChunk = 100;
        const lineLengthLimit = 20000;
        
        var boundProcessChunk = processChunk.bind(this);
        var processChunkInterval = setInterval(boundProcessChunk, 25);
        boundProcessChunk();
        
        function processChunk()
        {
            for (var i = 0; i < tokensPerChunk; i++) {
                if (this.cursor >= this.lineCode.length)
                    moveToNextLine.call(this);
                if (this.lineIndex >= rowsLength) {
                    this.sourceFrame.dispatchEventToListeners("syntax highlighting complete");
                    return;
                }
                if (this.cursor > lineLengthLimit) {
                    var codeFragment = this.lineCode.substring(this.cursor);
                    this.nonToken += codeFragment;
                    this.cursor += codeFragment.length;
                }

                this.lex();
            }
        }
        
        function moveToNextLine()
        {
            this.appendNonToken();
            
            var row = rows[this.lineIndex];
            var line = row ? row.cells[1] : null;
            if (line && this.newLine) {
                line.removeChildren();
                
                if (this.messageBubble)
                    this.newLine.appendChild(this.messageBubble);
                
                line.parentNode.insertBefore(this.newLine, line);
                line.parentNode.removeChild(line);
                
                this.newLine = null;
            }
            this.lineIndex++;
            if (this.lineIndex >= rowsLength && processChunkInterval) {
                clearInterval(processChunkInterval);
                this.sourceFrame.dispatchEventToListeners("syntax highlighting complete");
                return;
            }
            row = rows[this.lineIndex];
            line = row ? row.cells[1] : null;
            
            this.messageBubble = null;
            if (line.lastChild && line.lastChild.nodeType === Node.ELEMENT_NODE && line.lastChild.hasStyleClass("webkit-html-message-bubble")) {
                this.messageBubble = line.lastChild;
                line.removeChild(this.messageBubble);
            }

            this.lineCode = line.textContent;
            this.newLine = line.cloneNode(false);
            this.cursor = 0;
            if (!line)
                moveToNextLine();
        }
    },
    
    lex: function()
    {
        var token = null;
        var codeFragment = this.lineCode.substring(this.cursor);
        
        for (var i = 0; i < this.rules.length; i++) {
            var rule = this.rules[i];
            var ruleContinueStateCondition = ("preContinueState" in rule) ? rule.preContinueState : this.ContinueState.None;
            if (this.continueState !== ruleContinueStateCondition)
                continue;
            if (("preLexState" in rule) && this.lexState !== rule.preLexState)
                continue;

            var match = rule.pattern.exec(codeFragment);
            if (match) {
                token = match[0];
                if (token && (!rule.keywords || (token in rule.keywords))) {
                    this.cursor += token.length;
                    if (rule.style) {
                        this.appendNonToken();
                        var elem = this.createSpan(token, rule.style);
                        this.newLine.appendChild(elem);
                        if (rule.callback)
                            rule.callback.call(this, elem);
                    } else
                        this.nonToken += token;

                    if ("postLexState" in rule)
                        this.lexState = rule.postLexState;
                    if ("postContinueState" in rule)
                        this.continueState = rule.postContinueState;
                    return;
                }
            }
        }
        this.nonToken += codeFragment[0];
        this.cursor++;
    },
    
    appendNonToken: function ()
    {
        if (this.nonToken.length > 0) {
            this.newLine.appendChild(document.createTextNode(this.nonToken));
            this.nonToken = "";
        }
    },
    
    syntaxHighlightNode: function(node)
    {
        this.lineCode = node.textContent;
        node.removeChildren();
        this.newLine = node;
        this.cursor = 0;
        while (true) {
            if (this.cursor >= this.lineCode.length) {
                var codeFragment = this.lineCode.substring(this.cursor);
                this.nonToken += codeFragment;
                this.cursor += codeFragment.length;
                this.appendNonToken();
                this.newLine = null;
                return;
            }

            this.lex();
        }
    }
}
