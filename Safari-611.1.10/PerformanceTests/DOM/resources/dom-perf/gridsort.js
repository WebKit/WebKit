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

// GridSort
//
// This test is designed to test the performance of sorting a grid of nodes
// such as what you might use in a spreadsheet application.

// returns an array of integers from 0 to totalCells
function generateValuesArray(totalCells) {
    var values = [];
    for (var i = 0; i < totalCells; i++)
        values[i] = i;
    return values;
}

// creates value text nodes in a table using DOM methods
function populateTableUsingDom(tableId, width, height) {
    var table = document.getElementById(tableId);
    var values = generateValuesArray(width * height);

    for (var i = 0; i < height; i++) {
        var row = table.insertRow(i);
        for (var j = 0; j < width; j++) {
            var cell = row.insertCell(j);
            var valueIndex = Math.floor(Math.random() * values.length);
            var value = values.splice(valueIndex, 1);
            cell.appendChild(document.createTextNode(value));
        }
    }
}

// returns HTML string for rows/columns of table data
function getTableContentHTML(width, height) {
    var values = generateValuesArray(width * height);

    var html = []; // fragments we will join together at the end
    var htmlIndex  = 0;

    for (var i = 0; i < height; i++) {
        html.push("<tr>");
        for (var j = 0; j < width; j++) {
            html.push("<td>");
            var valueIndex = Math.floor(Math.random() * values.length);
            var value = values.splice(valueIndex, 1);
            html.push(value);
            html.push("</td>");
        }
        html.push("</tr>");
    }
    return html.join("");
}

// When sorting a table by a column, we create one of these for each
// cell in the column, and it keeps pointers to all the nodes in that
// row. This way we can sort an array of SortHelper objects, and then
// use the sibling node pointers to move all values in a row to their
// proper place according to the new sort order.
function SortHelper(row, index) {
    this.nodes = [];
    var numCells = row.cells.length;
    for (var i = 0; i < numCells; i++)
        this.nodes[i] = row.cells[i].firstChild;
    this.originalIndex = index;
}

function compare(a, b) {
    return a - b;
}

// sorts all rows of the table on a given column
function sortTableOnColumn(table, columnIndex) {
    var numRows = table.rows.length;
    var sortHelpers = [];
    for (var i = 0; i < numRows; i++)
        sortHelpers.push(new SortHelper(table.rows[i], i));

    // sort by nodeValue with original position breaking ties
    sortHelpers.sort(function(a, b) {
        var cmp = compare(Number(a.nodes[columnIndex].nodeValue), Number(b.nodes[columnIndex].nodeValue));
        if (cmp === 0)
            return compare(a.originalIndex, b.originalIndex);
        return cmp;
    });

    // now place all cells in their new position
    var numSortHelpers = sortHelpers.length;
    for (var i = 0; i < numSortHelpers; i++) {
        var helper = sortHelpers[i];
        if (i == helper.originalIndex)
            continue; // no need to move this row
        var columnCount = table.rows[i].cells.length;
        for (var j = 0; j < columnCount; j++) {
            var cell = table.rows[i].cells[j];
            if (cell.firstChild) {
                // a SortHelper will still have a reference to this node, so it
                // won't get orphaned/garbage collected
                cell.removeChild(cell.firstChild);
            }
            cell.appendChild(helper.nodes[j]);
        }
    }
}

function clearExistingTable() {
    var table = document.getElementById("gridsort_table");
    if (table) {
        // clear out existing table
        table.parentNode.removeChild(table);
    }
}

function createTableUsingDom() {
    clearExistingTable();
    var table = document.createElement("table");
    table.id = "gridsort_table";
    table.border = 1;
    document.getElementById("benchmark_content").appendChild(table);
    populateTableUsingDom("gridsort_table", 60, 60);
}

function createTableUsingInnerHtml() {
    clearExistingTable();
    var tableContent = getTableContentHTML(60, 60);
    var html = "<table id='gridsort_table' border='1'>" + tableContent + "</table>";
    document.getElementById("benchmark_content").innerHTML = html;
}

function sortTable() {
    var table = document.getElementById("gridsort_table");
    // TODO - it might be interesting to sort several (or all)
    // columns in succession, but for now that's fairly slow
    sortTableOnColumn(table, 0);
}

var GridSortTest = new BenchmarkSuite('GridSort', [
    new Benchmark("SortDomTable (60x60)", sortTable, createTableUsingDom, null, false),
    new Benchmark("SortInnerHtmlTable (60x60)", sortTable, createTableUsingInnerHtml, null, false)
]);
