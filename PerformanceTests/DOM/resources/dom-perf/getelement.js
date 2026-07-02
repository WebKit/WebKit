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

// Tests looking for ids in different DOM trees full of div elements.
function GetElementTest(size, appendStyle, treeStyle) {
    var me = this;
    this.size = size;
    this.appendStyle = appendStyle;
    this.ids = null;
    this.treeStyle = treeStyle;
    this.name = (this.Sizes.length > 1 ? (me.size + ", ") : "") + me.appendStyle + ", " + me.treeStyle;

    this.setupDOM = function() {
        var domTree = me.getDOMTree().cloneNode(true);
        this.suite.benchmarkContent.appendChild(domTree);
        me.nodesFound = 0;
        me.ids = me.getIds();
        return domTree;
    };

    this.setupHTML = function() {
        this.suite.benchmarkContent.innerHTML = me.getDOMTree().innerHTML;
        me.nodesFound = 0;
        me.ids = me.getIds();
    };

    if (this.appendStyle == "DOM")
        this.Setup = this.setupDOM;
    else
        this.Setup = this.setupHTML;

    this.Test = function(handle) {
        var kIterations = 1;
        for (var iterations = 0; iterations < kIterations; iterations++) {
            me.nodesFound = 0;
            for (var i = 0, len = me.ids.length; i < len; i++) {
                var div = document.getElementById(me.ids[i]);
                var nodeName = div.nodeName;
                me.nodesFound++;
            }
        }
    };

    this.Cleanup = function(handle) {
        var expectedCount = me.ids.length;
        if (me.nodesFound != expectedCount)
            throw "Wrong number of nodes found: " + me.nodesFound + " expected: " + expectedCount;
    };

    this.GetBenchmark = function() {
        return new Benchmark(this.name, this.Test, this.Setup, this.Cleanup);
    };

    this.getIdsFromTree = function(parent, maxNumberOfNodes) {
        var allDivs = parent.getElementsByTagName("div");
        var len = allDivs.length;
        var skip;
        if (maxNumberOfNodes >= allDivs.length)
            skip = 0;
        else
            skip = Math.floor(allDivs.length / maxNumberOfNodes) - 1;
        var ids = [];
        var l = 0;
        for (var i = 0, len = allDivs.length; i < len && l < maxNumberOfNodes; i += (skip + 1)) {
            var div = allDivs[i];
            ids.push(div.id);
            l++;
        }
        return ids;
    };

    this.createTreeAndIds = function() {
        var maxNumberOfNodes = 20000;
        var domTree;

        if (me.treeStyle == "dups") {
            // We use four of the trees for the dups style,
            // so they get too big if you use the full size for the bigger trees
            switch (me.size) {
            case "small":
                domTree = BenchmarkSuite.prototype.generateSmallTree();
                break;
            case "medium":
                domTree = BenchmarkSuite.prototype.generateDOMTree(15, 12, 4);
                break;
            case "large":
                domTree = BenchmarkSuite.prototype.generateDOMTree(26, 26, 1);
                break;
            }
        } else {
            switch (me.size) {
            case "small":
                domTree = BenchmarkSuite.prototype.generateSmallTree();
                break;
            case "medium":
                domTree = BenchmarkSuite.prototype.generateMediumTree();
                break;
            case "large":
                domTree = BenchmarkSuite.prototype.generateLargeTree();
                break;
            }
        }

        var allDivs = domTree.getElementsByTagName("*");
        var len = allDivs.length;
        var modBy;
        if (maxNumberOfNodes >= allDivs.length)
            modBy = 1;
        else
            modBy = Math.floor(allDivs.length / maxNumberOfNodes);
        var ids = [];
        for (var i = 0, len = allDivs.length; i < len; i++) {
            var mod = i % modBy;
            var div = allDivs[i];
            if (mod == 0 && ids.length < maxNumberOfNodes) {
                if (div.id && div.id != "")
                    ids.push(div.id);
            } else if (me.treeStyle == "sparse")
                div.id = null;
        }

        if (me.treeStyle == "dups") {
            var newRoot = document.createElement("div");
            for (var i = 0; i < 5; i++)
                newRoot.appendChild(domTree.cloneNode(true));
            domTree = newRoot;
        }

        var treeAndIds = {
            tree: domTree,
            ids: ids
        };
        return treeAndIds;
    };

    this.getTreeAndIds = function() {
        var treeAndIdsMap = me.TreeAndIds[me.size];
        if (!treeAndIdsMap) {
            treeAndIdsMap = {};
            me.TreeAndIds[me.size] = treeAndIdsMap;
        }
        var treeAndIds = treeAndIdsMap[me.treeStyle];
        if (!treeAndIds) {
            treeAndIds = me.createTreeAndIds();
            treeAndIdsMap[me.treeStyle] = treeAndIds;
        }
        return treeAndIds;
    };

    this.getDOMTree = function() {
        var treeAndIds = me.getTreeAndIds();
        return treeAndIds.tree;
    };

    this.getIds = function() {
        var treeAndIds = me.getTreeAndIds();
        return treeAndIds.ids;
    };
}

GetElementTest.prototype = {
    // Different sizes are possible, but we try to keep the runtime and memory
    // consumption reasonable.
    Sizes: ["medium"],
    TreeStyles: ["sparse", "dense", "dups"],
    TreeAndIds: {},
    AppendStyles: ["DOM", "HTML"]
};

// Generate the matrix of all benchmarks
var benchmarks = [];
GetElementTest.prototype.Sizes.forEach(function(size) {
    GetElementTest.prototype.AppendStyles.forEach(function(appendStyle) {
        GetElementTest.prototype.TreeStyles.forEach(function(treeStyle) {
            benchmarks.push(new GetElementTest(size, appendStyle, treeStyle).GetBenchmark());
        });
    });
});

var GetElementTest = new BenchmarkSuite("Get Elements", benchmarks);
