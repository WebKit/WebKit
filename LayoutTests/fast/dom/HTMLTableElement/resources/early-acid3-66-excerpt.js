description('An excerpt from an early Acid3 test 66: test the ordering and creation of rows');

var table = document.createElement('table');
var rows = [
    document.createElement('tr'),    // 0: ends up first child of the tfoot
    document.createElement('tr'),    // 1: goes at the end of the table
    document.createElement('tr'),    // 2: becomes second child of thead
    document.createElement('tr'),    // 3: becomes third child of the thead
    document.createElement('tr'),    // 4: not in the table
    table.insertRow(0),              // 5: not in the table
    table.createTFoot().insertRow(0) // 6: ends up second in the tfoot
];
rows[6].parentNode.appendChild(rows[0]);
table.appendChild(rows[1]);
table.insertBefore(document.createElement('thead'), table.firstChild);
table.firstChild.appendChild(rows[2]);
rows[2].parentNode.appendChild(rows[3]);
rows[4].appendChild(rows[5].parentNode);
table.insertRow(0);
table.tFoot.appendChild(rows[6]);

shouldBe("table.rows.length", "6");
shouldBe("table.getElementsByTagName('tr').length", "6");
shouldBe("table.childNodes.length", "3");
shouldBe("table.tHead", "table.childNodes[0]");
shouldBe("table.tHead.childNodes[0]", "table.getElementsByTagName('tr')[0]");
shouldBe("table.tHead.childNodes[1]", "table.getElementsByTagName('tr')[1]");
shouldBe("rows[2]", "table.getElementsByTagName('tr')[1]");
shouldBe("table.tHead.childNodes[2]", "table.getElementsByTagName('tr')[2]");
shouldBe("rows[3]", "table.getElementsByTagName('tr')[2]");
shouldBe("table.tFoot", "table.childNodes[1]");
shouldBe("table.tFoot.childNodes[0]", "table.getElementsByTagName('tr')[3]");
shouldBe("rows[0]", "table.getElementsByTagName('tr')[3]");
shouldBe("table.tFoot.childNodes[1]", "table.getElementsByTagName('tr')[4]");
shouldBe("rows[6]", "table.getElementsByTagName('tr')[4]");
shouldBe("table.childNodes[2]", "table.getElementsByTagName('tr')[5]");
shouldBe("rows[1]", "table.getElementsByTagName('tr')[5]");
shouldBe("table.tBodies.length", "0");

var successfullyParsed = true;
