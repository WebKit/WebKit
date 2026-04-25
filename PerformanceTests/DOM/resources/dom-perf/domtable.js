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

// DOMTable - a benchmark creating tables and accessing table elements
//
// This benchmark tests different mechanisms for creating an HTML table. By
// either creating the DOM elements individually or by creating an inner HTML
// as a string. The effect of forcing a layout is also measured.
// A second part of the benchmark sums the contents of all elements. Again
// in one set the benchmark is created using DOM functions from JavaScript,
// causing all nodes to be prewrapped, while in a second set the table is
// created using inner HTML which will wrap the elements at access.

// Size of the created tables.
var DOMTable_maxRows = 100;
var DOMTable_maxCols = 40;

// Helper variable to create consistent values for the table elements.
var DOMTable_element_count = 0;

// Functions needed to create a table by creating individual DOM elements.
function DOMTable_CreateCell(row_id, col_id) {
    var cell = document.createElement("td");
    cell.id = "$" + row_id + "$" + col_id;
    cell.textContent = DOMTable_element_count++;
    return cell;
}

function DOMTable_CreateRow(row_id, cols) {
    var row = document.createElement("tr");
    for (var i = 0; i < cols; i++)
        row.appendChild(DOMTable_CreateCell(row_id, i));
    return row;
}

function DOMTable_CreateTable(rows, cols) {
    var table = document.createElement("table");
    for (var i = 0; i < rows; i++)
        table.appendChild(DOMTable_CreateRow(i, cols));
    return table;
}

// Functions needed to create a table by creating a big piece of HTML in a
// single string.
function DOMTable_CreateCellIH(row_id, col_id) {
    return '<td id="$' + row_id + '$' + col_id + '">' + DOMTable_element_count++ + '</td>';
}

function DOMTable_CreateRowIH(row_id, cols) {
    var html_string = '<tr>';
    for (var i = 0; i < cols; i++)
        html_string += DOMTable_CreateCellIH(row_id, i);
    return html_string + '</tr>';
}

function DOMTable_CreateTableIH(rows, cols) {
    var html_string = '<table>';
    for (var i = 0; i < rows; i++)
        html_string += DOMTable_CreateRowIH(i, cols);
    return html_string + '</table>';
}


// Shared setup function for all table creation tests.
function DOMTable_CreateSetup() {
    DOMTable_element_count = 0;
    return document.getElementById("benchmark_content");
}

function DOMTable_Create(root_element) {
    // Create the table and add it to the root_element for the benchmark.
    root_element.appendChild(DOMTable_CreateTable(DOMTable_maxRows, DOMTable_maxCols));
    return root_element;
}

function DOMTable_CreateLayout(root_element) {
    // Create the table and add it to the root_element for the benchmark.
    root_element.appendChild(DOMTable_CreateTable(DOMTable_maxRows, DOMTable_maxCols));
    // Force a layout by requesting the height of the table. The result is
    // going to be ignored because there is not cleanup function registered.
    return root_element.scrollHeight;
}

function DOMTable_InnerHTML(root_element) {
    // Create the HTML string for the table and set it at the root_element for the benchmark.
    root_element.innerHTML = DOMTable_CreateTableIH(DOMTable_maxRows, DOMTable_maxCols);
    return root_element;
}

function DOMTable_InnerHTMLLayout(root_element) {
    // Create the HTML string for the table and set it at the root_element for the benchmark.
    root_element.innerHTML = DOMTable_CreateTableIH(DOMTable_maxRows, DOMTable_maxCols);
    // Force a layout by requesting the height of the table. The result is
    // going to be ignored because there is not cleanup function registered.
    return root_element.scrollHeight;
}

function DOMTableSum_Setup() {
    // Create the table to be summed using DOM operations from JavaScript. By
    // doing it this way the elements are all pre-wrapped.
    DOMTable_element_count = 0;
    var root_element = document.getElementById("benchmark_content");
    var table = DOMTable_CreateTable(DOMTable_maxRows, DOMTable_maxCols*5);
    root_element.appendChild(table);
    return root_element;
}

function DOMTableSum_SetupIH() {
    // Create the table to be summed using InnerHTML. By doing it this way the
    // elements need to be wrapped on access.
    DOMTable_element_count = 0;
    var root_element = document.getElementById("benchmark_content");
    var table = DOMTable_CreateTableIH(DOMTable_maxRows, DOMTable_maxCols*5);
    root_element.innerHTML = table;
    return root_element;
}

function DOMTableSum_ById(ignore) {
    // Sum all elements in the table by finding each element by its id.
    var sum = 0;
    var maxRows = DOMTable_maxRows;
    var maxCols = DOMTable_maxCols*5;
    for (var r = 0; r < maxRows; r++) {
        for (var c = 0; c < maxCols; c++) {
            var cell = document.getElementById("$"+r+"$"+c);
            sum += (+cell.textContent);
        }
    }
    return sum;
}

function DOMTableSum_ByTagName(root_element) {
    // Sum all elements in the table by getting a NodeList of all "td" elements.
    var sum = 0;
    var nodes = root_element.getElementsByTagName("td");
    var length = nodes.length;
    for (var i = 0; i < length; i++) {
        var cell = nodes[i];
        sum += (+cell.textContent);
    }
    return sum;
}

var DOMTableTest = new BenchmarkSuite('DOMTable', [
    new Benchmark("create", DOMTable_Create, DOMTable_CreateSetup),
    new Benchmark("create and layout", DOMTable_CreateLayout, DOMTable_CreateSetup),
    new Benchmark("create with innerHTML", DOMTable_InnerHTML, DOMTable_CreateSetup),
    new Benchmark("create and layout with innerHTML", DOMTable_InnerHTMLLayout, DOMTable_CreateSetup),
    new Benchmark("sum elements by id", DOMTableSum_ById, DOMTableSum_Setup),
    new Benchmark("sum elements by id with innerHTML", DOMTableSum_ById, DOMTableSum_SetupIH),
    new Benchmark("sum elements by tagname", DOMTableSum_ByTagName, DOMTableSum_Setup),
    new Benchmark("sum elements by tagname with innerHTML", DOMTableSum_ByTagName, DOMTableSum_SetupIH)
]);

