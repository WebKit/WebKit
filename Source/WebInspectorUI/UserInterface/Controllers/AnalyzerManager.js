/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

WI.AnalyzerManager = class AnalyzerManager extends WI.Object
{
    constructor()
    {
        super();

        this._eslintConfig = {
            env: {
                "browser": true,
                "node": false
            },
            globals: {
                "document": true
            },
            rules: {
                "consistent-return": 2,
                "curly": 0,
                "eqeqeq": 0,
                "new-parens": 0,
                "no-comma-dangle": 0,
                "no-console": 0,
                "no-constant-condition": 0,
                "no-extra-bind": 2,
                "no-extra-semi": 2,
                "no-proto": 0,
                "no-return-assign": 2,
                "no-trailing-spaces": 2,
                "no-underscore-dangle": 0,
                "no-unused-expressions": 2,
                "no-wrap-func": 2,
                "semi": 2,
                "space-infix-ops": 2,
                "space-return-throw-case": 2,
                "strict": 0,
                "valid-typeof": 2
            }
        };

        this._sourceCodeMessagesMap = new WeakMap;

        WI.SourceCode.addEventListener(WI.SourceCode.Event.ContentDidChange, this._handleSourceCodeContentDidChange, this);
    }

    // Public

    getAnalyzerMessagesForSourceCode(sourceCode)
    {
        return new Promise(function(resolve, reject) {
            var analyzer = WI.AnalyzerManager._typeAnalyzerMap.get(sourceCode.type);
            if (!analyzer) {
                reject(new Error("This resource type cannot be analyzed."));
                return;
            }

            if (this._sourceCodeMessagesMap.has(sourceCode)) {
                resolve(this._sourceCodeMessagesMap.get(sourceCode));
                return;
            }

            function retrieveAnalyzerMessages(properties)
            {
                var analyzerMessages = [];
                var rawAnalyzerMessages = analyzer.verify(sourceCode.content, this._eslintConfig);

                // Raw line and column numbers are one-based. SourceCodeLocation expects them to be zero-based so we subtract 1 from each.
                for (var rawAnalyzerMessage of rawAnalyzerMessages)
                    analyzerMessages.push(new WI.AnalyzerMessage(new WI.SourceCodeLocation(sourceCode, rawAnalyzerMessage.line - 1, rawAnalyzerMessage.column - 1), rawAnalyzerMessage.message, rawAnalyzerMessage.ruleId));

                this._sourceCodeMessagesMap.set(sourceCode, analyzerMessages);

                resolve(analyzerMessages);
            }

            sourceCode.requestContent().then(retrieveAnalyzerMessages.bind(this)).catch(handlePromiseException);
        }.bind(this));
    }

    sourceCodeCanBeAnalyzed(sourceCode)
    {
        return sourceCode.type === WI.Resource.Type.Script;
    }

    // Private

    _handleSourceCodeContentDidChange(event)
    {
        var sourceCode = event.target;

        // Since sourceCode has changed, remove it and its messages from the map so getAnalyzerMessagesForSourceCode will have to reanalyze the next time it is called.
        this._sourceCodeMessagesMap.delete(sourceCode);
    }
};

WI.AnalyzerManager._typeAnalyzerMap = new Map;

// <https://webkit.org/b/136515> Web Inspector: JavaScript source text editor should have a linter
// WI.AnalyzerManager._typeAnalyzerMap.set(WI.Resource.Type.Script, eslint);
