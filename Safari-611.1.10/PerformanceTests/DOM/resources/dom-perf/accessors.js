/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

// Accessors - measure access get/set properties on various objects.

// Accessors are generally pretty quick.  We loop on each accessor
// many times in order to crank up the time of the microbenchmark
// so that measurements are more substantial.  Values were chosen
// to make Firefox3 performance land in the 100-300ms range.
var kBigCount = 1500000;
var kLittleCount = 300000;
var kTinyCount = 30000;
var kVeryTinyCount = 5000;

var Accessors = {};

Accessors.WindowGet = function() {
    var nLoops = kLittleCount;
    for (var loop = 0; loop < nLoops; loop++)
        window.length;
}

Accessors.WindowSet = function() {
    var nLoops = kBigCount;
    for (var loop = 0; loop < kLittleCount; loop++)
        window.name = "title";
}

Accessors.RootGet = function() {
    var nLoops = kLittleCount;
    for (var loop = 0; loop < nLoops; loop++)
        length;
}

Accessors.RootSet = function() {
    var nLoops = kLittleCount;
    for (var loop = 0; loop < nLoops; loop++)
        title = "name";
}

Accessors.DocumentGet = function() {
    var nLoops = kLittleCount;
    for (var loop = 0; loop < nLoops; loop++)
        document.nodeType;
}

Accessors.DocumentSet = function() {
    var nLoops = kVeryTinyCount;
    for (var loop = 0; loop < nLoops; loop++)
        document.title = "name";
}

Accessors.DOMObjectSetup = function() {
    var o1 = document.createElement("span");
    var o2 = document.createElement("span");
    o1.appendChild(o2);
    this.suite.benchmarkContent.appendChild(o1);
    return o1;
}

Accessors.DOMObjectGet = function(o1) {
    var nLoops = kLittleCount;
    for (var loop = 0; loop < nLoops; loop++)
        o1.nodeType;
}

Accessors.DOMObjectSet = function(o1) {
    var nLoops = kLittleCount;
    var title = "title";
    for (var loop = 0; loop < nLoops; loop++)
        o1.title = title;
}

Accessors.ObjectGet = function(o1) {
    var nLoops = kBigCount;
    for (var loop = 0; loop < nLoops; loop++)
        o1.nodeType;
}

Accessors.NodeListSetup = function() {
    var o1 = document.createElement("span");
    var o2 = document.createElement("span");
    o1.appendChild(o2);
    this.suite.benchmarkContent.appendChild(o1);
    return o1.childNodes;
}

Accessors.NodeListGet = function(o1) {
    var nLoops = kLittleCount;
    for (var loop = 0; loop < nLoops; loop++)
        o1.length;
}

Accessors.CSSSetup = function() {
    var span = document.createElement("span");
    span.appendChild(document.createTextNode("test"));
    span.style.fontWeight = "bold";
    this.suite.benchmarkContent.appendChild(span);
    return span;
}
  

Accessors.CSSGet = function(span) {
    var nLoops = kLittleCount;
    for (var loop = 0; loop < nLoops; loop++)
        span.style.fontWeight;
}

Accessors.CSSSet = function(span) {
    var nLoops = kTinyCount;
    for (var loop = 0; loop < nLoops; loop++)
        span.style.fontWeight = "bold";
}

var AccessorsTest = new BenchmarkSuite('Accessors', [
    new Benchmark("CSS Style Get", Accessors.CSSGet, Accessors.CSSSetup),
    new Benchmark("CSS Style Set", Accessors.CSSSet, Accessors.CSSSetup),
    new Benchmark("Document Get NodeType", Accessors.DocumentGet),
    new Benchmark("Document Set Title", Accessors.DocumentSet),
    new Benchmark("Nodelist Get Length", Accessors.NodeListGet, Accessors.NodeListSetup),
    new Benchmark("Span Get NodeType", Accessors.DOMObjectGet, Accessors.DOMObjectSetup),
    new Benchmark("Span Set Title", Accessors.DOMObjectSet, Accessors.DOMObjectSetup),
    new Benchmark("Root Get Length", Accessors.RootGet),
    new Benchmark("Root Set Title", Accessors.RootSet),
    new Benchmark("Window Get Length", Accessors.WindowGet),
    new Benchmark("Window Set Name", Accessors.WindowSet)
]);
