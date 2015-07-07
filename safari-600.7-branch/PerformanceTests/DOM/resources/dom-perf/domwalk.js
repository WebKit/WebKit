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

// DOMWalk - a benchmark for walking the DOM
//
// This benchmark tests different mechanisms for touching every element
// in a section of the DOM. The elements are created from JavaScript causing
// them to be pre-wrapped.
//
// Tests small iterations and large iterations to see if there is a difference.


// We use two mechanisms to walk the DOM, and then
// verify that both yield the same result.
var __num_nodes = 0;

// We create a table to iterate as part of this test.
// For now it's just a big table with lots of elements.
function DOMWalk_CreateTable(height, width) {
    var table = document.getElementById("fake_dom");
    if (table) {
        // clear out existing table
        while (table.rows.length > 0)
            table.deleteRow(0);
    } else {
        var doc = document;
        table = doc.createElement("table");
        table.id = "fake_dom";
        table.border = 1;

        for (var row = 0; row < height; row++) {
            var row_object = doc.createElement("tr");
            for (var column = 0; column < width; column++) {
                var col_object = doc.createElement("td");
                var text = document.createTextNode(row.toString() + "." + column.toString());
                col_object.appendChild(text);
                row_object.appendChild(col_object);
            }
            table.appendChild(row_object);
        }
        var content = document.getElementById("benchmark_content");
        content.appendChild(table);
        var html = content.innerHTML;
        var width = content.clientWidth;
    }
    return table;
}

function DOMWalk_SetupSmall() {
    // Creates a table with 100 nodes.
    DOMWalk_CreateTable(10, 10);
}

function DOMWalk_SetupLarge() {
    // Creates a table with 4000 nodes.
    DOMWalk_CreateTable(200, 200);
}

// Walks the DOM via getElementsByTagName.
function DOMWalk_ByTagName(table) {
    var items = table.getElementsByTagName("*");
    var item;
    var length = items.length;
    for (var index = 0; index < length; index++)
        item = items[index];

    // Return the number of nodes seen.
    return items.length;
}

function DOMWalk_ByTagNameDriver(loops) {
    var table = document.getElementById("benchmark_content");
    for (var loop = 0; loop < loops; loop++)
        __num_nodes = DOMWalk_ByTagName(table);
}

function DOMWalk_ByTagNameSmall() {
    // This test runs in a short time.  We loop a few times in order to avoid small time measurements.
    DOMWalk_ByTagNameDriver(1000);
}

function DOMWalk_ByTagNameLarge() {
    DOMWalk_ByTagNameDriver(1);
}

function DOMWalk_Recursive(node) {
    // Count Element Nodes only.
    // IE does not define the Node constants.
    var count = (node.nodeType == /* Node.ELEMENT_NODE */ 1) ? 1 : 0;

    var child = node.firstChild;
    while (child !== null) {
        count += DOMWalk_Recursive(child);
        child = child.nextSibling;
    }

    return count;
}

// Walks the DOM via a recursive walk
function DOMWalk_RecursiveDriver(loops) {
    var table = document.getElementById("benchmark_content");
    for (var loop = 0; loop < loops; loop++) {
        var count = DOMWalk_Recursive(table) - 1;  // don't count the root node.
        if (count != __num_nodes || count === 0)
            throw "DOMWalk failed!  Expected " + __num_nodes + " nodes but found " + count + ".";
    }
}

function DOMWalk_RecursiveSmall() {
    // This test runs in a short time.  We loop a few times in order to avoid small time measurements.
    DOMWalk_RecursiveDriver(200);
}

function DOMWalk_RecursiveLarge() {
    // Only iterate once over the large DOM structures.
    DOMWalk_RecursiveDriver(1);
}


var DOMWalkTest = new BenchmarkSuite('DOMWalk', [
    new Benchmark("DOMWalkByTag (100 nodes)", DOMWalk_ByTagNameSmall, DOMWalk_SetupSmall),
    new Benchmark("DOMWalkRecursive (100 nodes)", DOMWalk_RecursiveSmall, DOMWalk_SetupSmall),
    new Benchmark("DOMWalkByTag (4000 nodes)", DOMWalk_ByTagNameLarge, DOMWalk_SetupLarge),
    new Benchmark("DOMWalkRecursive (4000 nodes)", DOMWalk_RecursiveLarge, DOMWalk_SetupLarge)
]);
