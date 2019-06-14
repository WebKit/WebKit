/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

CSSFormatter = class CSSFormatter
{
    constructor(sourceText, indentString = "    ")
    {
        this._success = false;

        this._sourceText = sourceText;

        this._builder = new FormatterContentBuilder(indentString);
        this._builder.setOriginalLineEndings(this._sourceText.lineEndings());

        this._format();

        this._success = true;
    }

    // Public

    get success() { return this._success; }

    get formattedText()
    {
        if (!this._success)
            return null;
        return this._builder.formattedContent;
    }

    get sourceMapData()
    {
        if (!this._success)
            return null;
        return this._builder.sourceMapData;
    }

    // Private

    _format()
    {
        const quoteTypes = new Set([`"`, `'`]);

        const dedentBefore = new Set([`}`]);

        const newlineBefore = new Set([`}`]);
        const newlineAfter = new Set([`{`, `}`, `;`]);

        const indentAfter = new Set([`{`]);

        const addSpaceBefore = new Set([`+`, `*`, `~`, `>`, `(`, `{`, `!`]);
        const addSpaceAfter = new Set([`,`, `+`, `*`, `~`, `>`, `)`, `:`]);

        const removeSpaceBefore = new Set([`,`, `(`, `)`, `}`, `:`, `;`]);
        const removeSpaceAfter = new Set([`(`, `{`, `}`, `,`, `!`, `;`]);

        const inAtRuleRegExp = /^\s*@[a-zA-Z][-a-zA-Z]+/;
        const inAtRuleBeforeParenthesisRegExp = /^\s*@[a-zA-Z][-a-zA-Z]+$/;
        const inAtRuleAfterParenthesisRegExp = /^\s*@[a-zA-Z][-a-zA-Z]+[^("':]*\([^"':]*:/;
        const inAtSupportsRuleRegExp = /^\s*@[a-zA-Z][-a-zA-Z]+[^"':]*:/;

        const lineStartCouldBePropertyRegExp = /^\s+[-_a-zA-Z][-_a-zA-Z0-9]*/;

        const lastTokenWasOpenParenthesisRegExp = /\(\s*$/;

        let depth = 0;

        for (let i = 0; i < this._sourceText.length; ++i) {
            let c = this._sourceText[i];

            let testCurrentLine = (regExp) => regExp.test(this._builder.currentLine);

            let inSelector = () => {
                let nextOpenBrace = this._sourceText.indexOf(`{`, i);
                if (nextOpenBrace !== -1) {
                    let nextQuote = Infinity;
                    for (let quoteType of quoteTypes) {
                        let quoteIndex = this._sourceText.indexOf(quoteType, i);
                        if (quoteIndex !== -1 && quoteIndex < nextQuote)
                            nextQuote = quoteIndex;
                    }
                    if (nextOpenBrace < nextQuote) {
                        let nextSemicolon = this._sourceText.indexOf(`;`, i);
                        if (nextSemicolon === -1)
                            nextSemicolon = Infinity;

                        let nextNewline = this._sourceText.indexOf(`\n`, i);
                        if (nextNewline === -1)
                            nextNewline = Infinity;

                        if (nextOpenBrace < Math.min(nextSemicolon, nextNewline))
                            return true;
                    }
                }

                if (testCurrentLine(lineStartCouldBePropertyRegExp))
                    return false;

                return true;
            };

            let inProperty = () => {
                if (!depth)
                    return false;
                return !testCurrentLine(inAtRuleRegExp) && !inSelector();
            };

            let formatBefore = () => {
                if (this._builder.lastNewlineAppendWasMultiple && c === `}`)
                    this._builder.removeLastNewline();

                if (dedentBefore.has(c))
                    this._builder.dedent();

                if (!this._builder.lastTokenWasNewline && newlineBefore.has(c))
                    this._builder.appendNewline();

                if (!this._builder.lastTokenWasWhitespace && addSpaceBefore.has(c)) {
                    let shouldAddSpaceBefore = () => {
                        if (c === `(`) {
                            if (testCurrentLine(inAtSupportsRuleRegExp))
                                return false;
                            if (!testCurrentLine(inAtRuleRegExp))
                                return false;
                        }
                        return true;
                    };
                    if (shouldAddSpaceBefore())
                        this._builder.appendSpace();
                }

                while (this._builder.lastTokenWasWhitespace && removeSpaceBefore.has(c)) {
                    let shouldRemoveSpaceBefore = () => {
                        if (c === `:`) {
                            if (!testCurrentLine(this._builder.currentLine.includes(`(`) ? inAtRuleRegExp : inAtRuleBeforeParenthesisRegExp)) {
                                if (!inProperty())
                                    return false;
                            }
                        }
                        if (c === `(`) {
                            if (!testCurrentLine(lastTokenWasOpenParenthesisRegExp)) {
                                if (testCurrentLine(inAtRuleRegExp) && !testCurrentLine(inAtRuleAfterParenthesisRegExp))
                                    return false;
                            }
                        }
                        return true;
                    };
                    if (!shouldRemoveSpaceBefore())
                        break;
                    this._builder.removeLastWhitespace();
                }
            };

            let formatAfter = () => {
                while (this._builder.lastTokenWasWhitespace && removeSpaceAfter.has(c)) {
                    let shouldRemoveSpaceAfter = () => {
                        if (c === `(`) {
                            if (!testCurrentLine(lastTokenWasOpenParenthesisRegExp)) {
                                if (!testCurrentLine(inAtRuleRegExp)) {
                                    if (!inProperty())
                                        return false;
                                }
                            }
                        }
                        return true;
                    };
                    if (!shouldRemoveSpaceAfter())
                        break;
                    this._builder.removeLastWhitespace();
                }

                if (!this._builder.lastTokenWasWhitespace && addSpaceAfter.has(c)) {
                    let shouldAddSpaceAfter = () => {
                        if (c === `:`) {
                            if (!testCurrentLine(this._builder.currentLine.includes(`(`) ? inAtRuleRegExp : inAtRuleBeforeParenthesisRegExp)) {
                                if (!inProperty())
                                    return false;
                            }
                        }
                        if (c === `)`) {
                            if (!testCurrentLine(inAtRuleRegExp)) {
                                if (!inProperty())
                                    return false;
                            }
                        }
                        return true;
                    };
                    if (shouldAddSpaceAfter())
                        this._builder.appendSpace();
                }

                if (indentAfter.has(c))
                    this._builder.indent();

                if (newlineAfter.has(c)) {
                    if (c === `}`)
                        this._builder.appendMultipleNewlines(2);
                    else
                        this._builder.appendNewline();
                }
            };

            let specialSequenceEnd = null;
            if (quoteTypes.has(c))
                specialSequenceEnd = c;
            else if (c === `/` && this._sourceText[i + 1] === `*`)
                specialSequenceEnd = `*/`;
            else if (c === `u` && this._sourceText[i + 1] === `r` && this._sourceText[i + 2] === `l` && this._sourceText[i + 3] === `(`)
                specialSequenceEnd = `)`;

            if (specialSequenceEnd) {
                let endIndex = i;
                do {
                    endIndex = this._sourceText.indexOf(specialSequenceEnd, endIndex + specialSequenceEnd.length);
                } while (endIndex !== -1 && this._sourceText[endIndex - 1] === `\\`);

                if (endIndex === -1)
                    endIndex = this._sourceText.length;

                this._builder.appendToken(this._sourceText.substring(i, endIndex + specialSequenceEnd.length), i);
                i = endIndex + specialSequenceEnd.length - 1; // Account for the iteration of the for loop.

                c = this._sourceText[i];
                formatAfter();
                continue;
            }

            if (/\s/.test(c)) {
                if (c === `\n` && !this._builder.lastTokenWasNewline) {
                    while (this._builder.lastTokenWasWhitespace)
                        this._builder.removeLastWhitespace();
                    if (!removeSpaceAfter.has(this._builder.lastToken))
                        this._builder.appendNewline();
                    else
                        this._builder.appendSpace();
                } else if (!this._builder.lastTokenWasWhitespace && !removeSpaceAfter.has(this._builder.lastToken))
                    this._builder.appendSpace();
                continue;
            }

            if (c === `{`)
                ++depth;
            else if (c === `}`)
                --depth;

            formatBefore();
            this._builder.appendToken(c, i);
            formatAfter();
        }

        this._builder.finish();
    }
};
