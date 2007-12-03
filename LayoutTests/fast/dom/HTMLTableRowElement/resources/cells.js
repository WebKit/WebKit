description('Test behavior of the HTMLTableRowElement cells attribute in cases where there is unusual nesting.');

function checkCellNesting(tag)
{
    var row = document.createElement("tr");
    var container = document.createElement(tag);
    var cell = document.createElement("td");
    row.appendChild(container);
    container.appendChild(cell);
    return row.cells.length;
}

function checkHeaderCellNesting(tag)
{
    var row = document.createElement("tr");
    var container = document.createElement(tag);
    var cell = document.createElement("th");
    row.appendChild(container);
    container.appendChild(cell);
    return row.cells.length;
}

var tags = [
    "col",
    "colgroup",
    "div",
    "form",
    "script",
    "table",
    "tbody",
    "tfoot",
    "thead",
    "tr",
];

for (i = 0; i < tags.length; ++i)
    shouldBe('checkCellNesting("' + tags[i] + '")', '0');

debug('');

shouldBe('checkCellNesting("td")', '1');
shouldBe('checkCellNesting("th")', '1');

debug('');

for (i = 0; i < tags.length; ++i)
    shouldBe('checkHeaderCellNesting("' + tags[i] + '")', '0');

debug('');

shouldBe('checkHeaderCellNesting("td")', '1');
shouldBe('checkHeaderCellNesting("th")', '1');

debug('');

var successfullyParsed = true;
