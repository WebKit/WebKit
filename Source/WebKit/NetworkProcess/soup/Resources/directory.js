
/* This code is based on Firefox directory listing, distributed under
 * Mozilla Public License.
 */

'use strict';

var gTable, gOrderBy, gTBody, gRows;

document.addEventListener("DOMContentLoaded", function() {
  gTable = document.getElementsByTagName("table")[0];
  gTBody = gTable.tBodies[0];
  if (gTBody.rows.length < 2)
    return;
  var headCells = gTable.tHead.rows[0].cells;

  function rowAction(i) {
    return function(event) {
      event.preventDefault();
      orderBy(i);
    }
  }

  for (var i = headCells.length - 1; i >= 0; i--) {
    var anchor = document.createElement("a");
    anchor.href = "";
    anchor.appendChild(headCells[i].firstChild);
    headCells[i].appendChild(anchor);
    headCells[i].addEventListener("click", rowAction(i), true);
  }
  gTable.setAttribute("order", "");
  orderBy(0);
}, "false");

function compareRows(rowA, rowB) {
  var a = rowA.cells[gOrderBy].getAttribute("sortable-data") || rowA.cells[gOrderBy].innerHTML;
  var b = rowB.cells[gOrderBy].getAttribute("sortable-data") || rowB.cells[gOrderBy].innerHTML;
  var intA = +a;
  var intB = +b;
  if (a == intA && b == intB) {
    a = intA;
    b = intB;
  } else {
    a = a.toLowerCase();
    b = b.toLowerCase();
  }
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  return 0;
}

function orderBy(column) {
  if (!gRows)
    gRows = Array.from(gTBody.rows);
  var order;
  if (gOrderBy == column) {
    order = gTable.getAttribute("order") == "asc" ? "desc" : "asc";
  } else {
    order = "asc";
    gOrderBy = column;
    gTable.setAttribute("order-by", column);
    gRows.sort(compareRows);
  }
  gTable.removeChild(gTBody);
  gTable.setAttribute("order", order);
  if (order == "asc")
    for (var i = 0; i < gRows.length; i++)
      gTBody.appendChild(gRows[i]);
  else
    for (var i = gRows.length - 1; i >= 0; i--)
      gTBody.appendChild(gRows[i]);
  gTable.appendChild(gTBody);
}

