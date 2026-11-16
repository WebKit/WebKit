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

// DOMDivWalk
// Tests iteration of a DOM tree which is comprised of a hierarchical
// set of div sections.
//
// Two mechanisms are used to iterate the DOM tree:
//    - recursive iteration via Node.children
//    - document.getElementsByTagName
//
// Various tree sizes are tested:
//    - small, medium, and large DOMs
//
// Because this test is run repeatedly on the same DOM tree, the first
// iteration may cause creation of the JS DOM Objects, and subsequent
// iterations will use the cached instance of that JS Object.
//

// These tests try walking over different DOM trees full of divs
// First create a DDWalkTest, using the above values,
// then call GetBenchmark()
function DDWalkTest(size, appendStyle, traverseStyle) {
    var me = this;
    this.size = size;
    this.appendStyle = appendStyle;
    this.traverseStyle = traverseStyle;
    this.nodesFound = 0;

    this.name = (this.Sizes.length > 1 ? (me.size + ", ") : "") + me.appendStyle + ", " + me.traverseStyle;

    this.setupDOM = function() {
        var domTree = me.getDOMTree().cloneNode(true);
        this.suite.benchmarkContent.appendChild(domTree);
        me.nodesFound = 0;
        return domTree;
    };

    this.setupHTML = function() {
        this.suite.benchmarkContent.innerHTML = me.getDOMTree().innerHTML;
        me.nodesFound = 0;
    };
  
    if (this.appendStyle == "DOM")
        this.Setup = this.setupDOM;
    else
        this.Setup = this.setupHTML;
  
    this.recurseChildNodes = function(handle) {
        function walkSubtree(parent, depth) {
            if (me.nodesFound == me.maxNodesFound)
                return;
            else {
                if (parent.nodeName == "SPAN" && depth > 0)
                    me.nodesFound++;
                var children = parent.childNodes;
                var nChildren = children.length;
                while (nChildren-- > 0) {
                    var child = children[nChildren];
                    walkSubtree(child, depth + 1);
                }
            }
        }
        walkSubtree(this.suite.benchmarkContent, 0);
    };
  
    this.recurseSiblings = function(handle) {
        function walkSubtree(parent, depth) {
            if (me.nodesFound == me.maxNodesFound)
                return;
            else {
                if (parent.nodeName == "SPAN" && depth > 0)
                    me.nodesFound++;
                for (var child = parent.firstChild; child !== null; child = child.nextSibling)
                  walkSubtree(child, depth + 1);
            }
        }
        walkSubtree(this.suite.benchmarkContent, 0);
    };

    this.getElements = function(handle) {
        var kIterations = 10;

        for (var iterations = 0; iterations < kIterations; iterations++) {
            me.nodesFound = 0;
            var items = this.suite.benchmarkContent.getElementsByTagName("span");
            for (var index = 0, length = items.length; index < length; ++index) {
                var item = items[index];
                var nodeName = item.nodeName;
                ++me.nodesFound;
                if (me.nodesFound == me.maxNodesFound)
                    return;
            }
        }
    };

    this.Test = this[this.traverseStyle];

    this.Cleanup = function(handle) {
        var expectedCount = me.ExpectedCounts[me.size];
        if (me.nodesFound != expectedCount)
            throw "Wrong number of nodes found: " + me.nodesFound + " expected: " + expectedCount;
    };

    this.GetBenchmark = function() {
        return new Benchmark(this.name, this.Test, this.Setup, this.Cleanup);    
    };

    this.getDOMTree = function() {
        var domTree = me.Trees[me.size];
        if (!domTree) {
            switch (me.size) {
            case "small" : domTree = BenchmarkSuite.prototype.generateSmallTree(); break;
            case "medium" : domTree = BenchmarkSuite.prototype.generateMediumTree(); break;
            case "large" : domTree = BenchmarkSuite.prototype.generateLargeTree(); break;
            }
            me.Trees[me.size] = domTree;
        }
        return domTree;
    };
}

DDWalkTest.prototype = {
    // Different sizes are possible, but we try to keep the runtime and memory
    // consumption reasonable.
    Sizes: ["medium"],
    Trees: {},
    AppendStyles: ["DOM", "HTML"],
    TraverseStyles: ["recurseChildNodes", "recurseSiblings", "getElements"],
    ExpectedCounts : {"small" : 90, "medium": 2106, "large" : 2000},

    // Limits the number of nodes to this number
    // If we don't do this the tests can take too long to run
    maxNodesFound : 10000
};


// Generate the matrix of all benchmarks
var benchmarks = [];
DDWalkTest.prototype.Sizes.forEach(function(size) {
    DDWalkTest.prototype.AppendStyles.forEach(function(appendStyle) {
        DDWalkTest.prototype.TraverseStyles.forEach(function(traverseStyle) {
            benchmarks.push(new DDWalkTest(size, appendStyle, traverseStyle).GetBenchmark());
        });
    });
});

var DOMDivWalkTest = new BenchmarkSuite("DOMDivWalk", benchmarks);
