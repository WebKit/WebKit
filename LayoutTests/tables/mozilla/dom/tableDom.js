/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var count = 1;
function genName(prefix) {
  return "X" + count++ + "\n";
}

function appendCell(aRow, aRowSpan, aColSpan) {
  var cell = document.createElement("TD", null);
  cell.rowSpan = aRowSpan;
  cell.colSpan = aColSpan;
  var text = document.createTextNode(genName());
  cell.appendChild(text);
  aRow.appendChild(cell);
}

function appendCellAt(aRowIndex, aRowSpan, aColSpan) {
dump(aRowIndex);
  var row = document.getElementsByTagName("TR")[aRowIndex];
  appendCell(row, aRowSpan, aColSpan);
}

function insertCell(aRow, aColIndex, aRowSpan, aColSpan) {
  var cells = aRow.cells;
  var refCell = cells.item(aColIndex);
  var newCell = document.createElement("TD", null);
  newCell.rowSpan = aRowSpan;
  newCell.colSpan = aColSpan;
  var text = document.createTextNode(genName());
  newCell.appendChild(text);
  aRow.insertBefore(newCell, refCell);
  //dump("SCRIPT: inserted  CELL as first cell in first row\n");
}

function insertCellAt(aRowIndex, aColIndex, aRowSpan, aColSpan) {
  var row = document.getElementsByTagName("TR")[aRowIndex];
  insertCell(row, aColIndex, aRowSpan, aColSpan);
}

function deleteCell(aRow, aColIndex) {
  aRow.deleteCell(aColIndex);
}

function deleteCellAt(aRowIndex, aColIndex) {
  var row = document.getElementsByTagName("TR")[aRowIndex];
  deleteCell(row, aColIndex);
}

//function appendRow(aRowGroup) {
//  var row = document.createElement("TR", null);
//  cell = document.createElement("TD", null);
//  row.appendChild(cell);
//  aRowGroup.appendChild(row);
//}

function appendRow(aRowGroup) {
  var row = document.createElement("TR", null);
  cell = document.createElement("TD", null);
  aRowGroup.appendChild(row);
  //row.appendChild(cell);
  //appendCell(row, 1, 1);
}

function appendRowAt(aRowGroupIndex) {
  var rowGroup = document.getElementsByTagName("TBODY")[aRowGroupIndex];
  appendRow(rowGroup);
}

function insertRow(aRowGroup, aRowIndex) {
  var rows = aRowGroup.rows;
  var refRow = rows.item(aRowIndex);
  var row = document.createElement("TR", null);
  aRowGroup.insertBefore(row, refRow);
  //appendCell(row, 1, 1);
}

function insertRowAt(aRowGroupIndex, aRowIndex) {
  var rowGroup = document.getElementsByTagName("TBODY")[aRowGroupIndex];
  insertRow(rowGroup, aRowIndex);
}

function deleteRow(aRowGroup, aRowIndex) {
  aRowGroup.deleteRow(aRowIndex);
}

function deleteRowAt(aRowGroupIndex, aRowIndex) {
  var row = document.getElementsByTagName("TBODY")[aRowGroupIndex];
  deleteRow(row, aRowIndex);
}

function insertTbody(aTable, aTbodyIndex) {
  var tbodies = aTable.tBodies;
  var refTbody = tbodies.item(aTbodyIndex);
  var tbody = document.createElement("TBODY", null);
  aTable.insertBefore(tbody, refTbody);
}

function insertTbodyAt(aTableIndex, aTbodyIndex) {
  var table = document.getElementsByTagName("TABLE")[aTableIndex];
  insertTbodyAt(table, aTbodyIndex);
}

function deleteTbody(aTable, aTbodyIndex) {
  var tbodies = aTable.tBodies;
  var tbody = tbodies.item(aTbodyIndex);
  aTable.removeChild(tbody);
}

function deleteTbodyAt(aTableIndex, aTbodyIndex) {
  var table = document.getElementsByTagName("TABLE")[aTableIndex];
  deleteTbody(table, aTbodyIndex);
}

function buildTable(aNumRows, aNumCols) {
  var table = document.getElementsByTagName("TABLE")[0];
  for (rowX = 0; rowX < aNumRows; rowX++) {
    var row = document.createElement("TR", null);
	  for (colX = 0; colX < aNumCols; colX++) {
      var cell = document.createElement("TD", null);
      var text = document.createTextNode(genName());
      cell.appendChild(text);
      row.appendChild(cell);
	}
	table.appendChild(row);
  }
}

