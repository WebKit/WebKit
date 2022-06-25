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

// CreateNodes
// Test mechanisms for creating and inserting nodes into a DOM tree.
//
// There are many ways to add nodes into a DOM tree.  This test exercises
// use of:
//    - createElement()/appendChild()
//    - createDocumentFragment()
//    - innerHTML()
//    - spans

function CreateNodes() {}

CreateNodes.nIterations = 10000;
CreateNodes.nIterationsSlow = 2000;
CreateNodes.currentIterations = CreateNodes.nIterations;
CreateNodes.nodeProto = null;

CreateNodes.createNode = function() {
    if (!CreateNodes.nodeProto) {
        var html = this.getNodeHTML();
        var span = document.createElement("span");
        span.innerHTML = html;
        CreateNodes.nodeProto = span.firstChild;
    }
    return CreateNodes.nodeProto;
};

CreateNodes.getNodeHTML = function() {
    return "<div style=\"font-style:bold\">test</div>";
};

CreateNodes.appendNodesWithDOM = function(node) {
    var nodes = CreateNodes.createNodesWithDOM(node);
    for (var i = 0; i < CreateNodes.nIterations; i++)
        this.suite.benchmarkContent.appendChild(nodes[i]);
    CreateNodes.forceNode(this);
};

CreateNodes.justAppendNodesWithDOM = function(nodes) {
    for (var i = 0; i < CreateNodes.nIterations; i++)
        this.suite.benchmarkContent.appendChild(nodes[i]);
};

CreateNodes.createNodesWithHTML = function(html) {
    var allHTML = "";
    for (var i = 0; i < CreateNodes.currentIterations; i++)
        allHTML += html;
    return allHTML;
};

CreateNodes.checkNodes = function() {
    var length = this.suite.benchmarkContent.childNodes.length;
    var count = CreateNodes.currentIterations;
    if (length != count)
        throw "Should have " + count + " nodes, have: " + length;
};

// Ensures that the node has really been created and not just delayed
CreateNodes.forceNode = function(benchmark) {
    var child =  benchmark.suite.benchmarkContent.childNodes[CreateNodes.nIterations / 2];
};

CreateNodes.appendNodesWithHTML = function(html) {
    var allHTML = CreateNodes.createNodesWithHTML(html);
    this.suite.benchmarkContent.innerHTML = allHTML;
    CreateNodes.forceNode(this);
};

CreateNodes.createNodesWithDOM = function(node) {
    var nodes = [];
    for (var i = 0; i < CreateNodes.nIterations; i++)
        nodes.push(node.cloneNode(true));
    return nodes;
};

CreateNodes.createNodesWithDOMSetup = function() {
    return createNodesWithDOM(createNode());
};

CreateNodes.createNodeSetup = function() {
    CreateNodes.currentIterations = CreateNodes.nIterations;
    return CreateNodes.createNode();
};

CreateNodes.createNodesWithHTMLUsingSpans = function(html) {
    var spans = [];
    for (var i = 0; i < CreateNodes.currentIterations; i++) {
        var spanNode = document.createElement("span");
        spanNode.innerHTML = html;
        spans.push(spanNode);
    }
    return spans;
};

CreateNodes.appendNodesWithHTMLUsingSpans = function(html) {
    var spans = CreateNodes.createNodesWithHTMLUsingSpans(html);
    for (var i = 0; i < CreateNodes.currentIterations; i++)
        this.suite.benchmarkContent.appendChild(spans[i]);
    CreateNodes.forceNode(this);
};

CreateNodes.appendNodesWithHTMLUsingDocumentFragments = function(html) {
    var fragments = CreateNodes.createNodesWithHTMLUsingDocumentFragments(html);
    for (var i = 0; i < CreateNodes.nIterations; i++)
        this.suite.benchmarkContent.appendChild(fragments[i]);
    CreateNodes.forceNode(this);
};

CreateNodes.createNodesWithHTMLUsingDocumentFragments = function(html) {
    var fragments = [];
    for (var i = 0; i < CreateNodes.nIterations; i++) {
        var fragment = document.createDocumentFragment();
        fragment.innerHTML = html;
        fragments.push(fragment);
    }
    return fragments;
};

CreateNodes.appendNodesWithDOMUsingDocumentFragment = function(node) {
    var fragment = CreateNodes.createNodesWithDOMUsingDocumentFragment(node);
    this.suite.benchmarkContent.appendChild(fragment);
    CreateNodes.forceNode(this);
};

CreateNodes.appendNodesWithDOMUsingSharedDocumentFragment = function(fragment) {
    this.suite.benchmarkContent.appendChild(fragment.cloneNode(true));
    CreateNodes.forceNode(this);
};

CreateNodes.createNodesWithDOMUsingDocumentFragment = function(node) {
    var nodes = CreateNodes.createNodesWithDOM(node);
    var fragment = document.createDocumentFragment();
    for (var i = 0; i < CreateNodes.nIterations; i++)
        fragment.appendChild(nodes[i]);
    return fragment;
};

CreateNodes.createSharedDocumentFragment = function() {
    var nodes = CreateNodes.createNodesWithDOM(CreateNodes.createNode());
    var fragment = document.createDocumentFragment();
    for (var i = 0; i < CreateNodes.nIterations; i++)
        fragment.appendChild(nodes[i]);
    return fragment;
};

CreateNodes.createHTMLSetup = function() {
    CreateNodes.currentIterations = CreateNodes.nIterationsSlow;
    return CreateNodes.getNodeHTML();
};

CreateNodes.createIFramesHTML = function() {
    var html = [];
    for (var i = 0; i < 100; i++)
        html.push("<iframe src='blank.html'></iframe>");
    return html.join('');
}

CreateNodes.appendIFramesHTML = function(html) {
    this.suite.benchmarkContent.innerHTML = html;
}

CreateNodes.createIFramesDOM = function() {
    var nodes = [];
    for (var i = 0; i < 100; i++) {
        var iframe = document.createElement('iframe');
        iframe.src = 'blank.html';
        nodes.push(iframe);
    }
    return nodes;
}

CreateNodes.appendIFramesDOM = function(nodes) {
    var content = this.suite.benchmarkContent;
    for (var i = 0, l = nodes.length; i < l; i++)
        content.appendChild(nodes[i]);
}

var CreateNodesTest = new BenchmarkSuite('CreateNodes', [
    new Benchmark("append, DOM, DocumentFragment",
        CreateNodes.appendNodesWithDOMUsingDocumentFragment, CreateNodes.createNodeSetup, CreateNodes.checkNodes),
    new Benchmark("create, DOM, DocumentFragment",
        CreateNodes.createNodesWithDOMUsingDocumentFragment, CreateNodes.createNodeSetup),
    new Benchmark("append, DOM, SharedDocumentFragment",
        CreateNodes.appendNodesWithDOMUsingSharedDocumentFragment, CreateNodes.createSharedDocumentFragment,  CreateNodes.checkNodes),
    new Benchmark("create, DOM",
        CreateNodes.createNodesWithDOM, CreateNodes.createNodeSetup),
    new Benchmark("append, DOM", 
        CreateNodes.appendNodesWithDOM, CreateNodes.createNodeSetup, CreateNodes.checkNodes),
    new Benchmark("append, DOM, iFrame",
        CreateNodes.appendIFramesDOM, CreateNodes.createIFramesDOM),
    new Benchmark("append, HTML", 
        CreateNodes.appendNodesWithHTML, CreateNodes.createHTMLSetup, CreateNodes.checkNodes),
    new Benchmark("create, HTML, Spans", 
        CreateNodes.createNodesWithHTMLUsingSpans, CreateNodes.createHTMLSetup),
    new Benchmark("append, HTML, Spans",
        CreateNodes.appendNodesWithHTMLUsingSpans, CreateNodes.createHTMLSetup, CreateNodes.checkNodes),
    new Benchmark("append, HTML, iFrame",
        CreateNodes.appendIFramesHTML, CreateNodes.createIFramesHTML)
]);
