description('An excerpt from an early Acid3 test 65: construct a table, and see if the table is as expected');

var table = document.createElement('table');
table.appendChild(document.createElement('tbody'));
var tr1 = document.createElement('tr');
table.appendChild(tr1);
table.appendChild(document.createElement('caption'));
table.appendChild(document.createElement('thead'));
// <table><tbody/><tr/><caption/><thead/>
table.insertBefore(table.firstChild.nextSibling, null); // move the <tr/> to the end
// <table><tbody/><caption/><thead/><tr/>
table.replaceChild(table.firstChild, table.lastChild); // move the <tbody/> to the end and remove the <tr>
// <table><caption/><thead/><tbody/>
var tr2 = table.tBodies[0].insertRow(0);
// <table><caption/><thead/><tbody><tr/></tbody>
shouldBe("table.tBodies[0].rows[0].rowIndex", "0");
shouldBe("table.tBodies[0].rows[0].sectionRowIndex", "0");
shouldBe("table.childNodes.length", "3");
shouldBe("!!table.caption", "true");
shouldBe("!!table.tHead", "true");
shouldBe("table.tFoot", "null");
shouldBe("table.tBodies.length", "1");
shouldBe("table.rows.length", "1");
shouldBe("tr1.parentNode", "null");
shouldBe("table.caption", "table.createCaption()");
shouldBe("table.tFoot", "null");
shouldBe("table.tHead", "table.createTHead()");
shouldBe("table.createTFoot()", "table.tFoot");
// either: <table><caption/><thead/><tbody><tr/></tbody><tfoot/>
//     or: <table><caption/><thead/><tfoot/><tbody><tr/></tbody>
table.tHead.appendChild(tr1);
// either: <table><caption/><thead><tr/></thead><tbody><tr/></tbody><tfoot/>
//     or: <table><caption/><thead><tr/></thead><tfoot/><tbody><tr/></tbody>
shouldBe("table.rows[0]", "table.tHead.firstChild");
shouldBe("table.rows.length", "2");
shouldBe("table.rows[1]", "table.tBodies[0].firstChild");

var successfullyParsed = true;
