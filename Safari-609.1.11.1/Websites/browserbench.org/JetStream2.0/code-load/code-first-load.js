/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

let indirectEval = eval;
class Benchmark {
    constructor(iterations) {

        let inspectorText;
        if (isInBrowser) {
            let request = new XMLHttpRequest();
            request.open('GET', inspectorPayloadBlob, false);
            request.send(null);
            if (!request.responseText.length)
                throw new Error("Expect non-empty sources");

            inspectorText = request.responseText;
        } else
            inspectorText = readFile("./code-load/inspector-payload-minified.js");

        this.inspectorText = `let _____top_level_____ = ${Math.random()}; ${inspectorText}`;

        this.index = 0;
    }

    prepareForNextIteration() {
        this.text = `function test${this.index}() { ${this.inspectorText} }`;
        if (this.text[0] !== "f")
            throw new Error;
        if (this.text[this.text.length - 1] !== "}")
            throw new Error;
        ++this.index;
    }

    runIteration() {
        indirectEval(this.text);
    }
}
