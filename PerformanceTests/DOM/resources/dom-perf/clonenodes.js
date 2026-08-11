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

// Tests cloning and appending small and medium DOM trees.
function CloneNodes(size, style) {
    var me = this;
    this.size = size;
    this.style = style;

    this.name = me.size + ', ' + me.style;

    var nClones = 0;

    this.Setup = function() {
        // getDOMTree will initialize the global tree if necessary.
        me.domTree = me.getDOMTree();
        if (me.style == 'append')
            me.domTree = me.domTree.cloneNode(true);
    };

    this.Test = function() {
        var kIterations = 10;
        if (me.style == 'clone') {
            for (var iterations = 0; iterations < kIterations; iterations++)
                me.domTree.cloneNode(true);
        } else {
            for (var iterations = 0; iterations < kIterations; iterations++)
                this.suite.benchmarkContent.appendChild(me.domTree);
        }
    };

    // Returns a tree of size me.size from the pool in the prototype, creating 
    // one if needed.
    this.getDOMTree = function() {
        var domTree = me.Trees[me.size];
        if (!domTree) {
            switch (this.size) {
            case 'small':
                domTree = BenchmarkSuite.prototype.generateSmallTree();
                break;
            case 'medium':
                domTree = BenchmarkSuite.prototype.generateMediumTree();
                break;
            case 'large':
                domTree = BenchmarkSuite.prototype.generateLargeTree();
                break;
            }
            me.Trees[me.size] = domTree;
        }
        return domTree;
    };

    this.GetBenchmark = function() {
        return new Benchmark(this.name, this.Test, this.Setup);
    };
};

CloneNodes.prototype = {
    Sizes: ['small', 'medium'],
    Styles: ['clone', 'append'],
    Trees: {}
};

// Generate a test for each size/style combination.
var benchmarks = [];
CloneNodes.prototype.Sizes.forEach(function(size) {
    CloneNodes.prototype.Styles.forEach(function(style) {
        benchmarks.push(new CloneNodes(size, style).GetBenchmark());
    });
});

var CloneNodesTest = new BenchmarkSuite('CloneNodes', benchmarks);
