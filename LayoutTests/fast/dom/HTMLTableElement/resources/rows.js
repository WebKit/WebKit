description('Test behavior of the HTMLTableElement rows attribute in cases where there is unusual nesting.');

function checkNoBodyRowNesting(tag)
{
    var table = document.createElement("table");
    var container = document.createElement(tag);
    var row = document.createElement("tr");
    table.appendChild(container);
    container.appendChild(row);
    return table.rows.length;
}

function checkRowNesting(tag)
{
    var table = document.createElement("table");
    var body = document.createElement("tbody");
    var container = document.createElement(tag);
    var row = document.createElement("tr");
    table.appendChild(body);
    body.appendChild(container);
    container.appendChild(row);
    return table.rows.length;
}

var sectionTags = [
    "tbody",
    "tfoot",
    "thead",
];

var otherTags = [
    "col",
    "colgroup",
    "div",
    "form",
    "script",
    "table",
    "td",
    "th",
];

for (i = 0; i < otherTags.length; ++i)
    shouldBe('checkRowNesting("' + otherTags[i] + '")', '0');

debug('');

for (i = 0; i < sectionTags.length; ++i)
    shouldBe('checkRowNesting("' + sectionTags[i] + '")', '0');

debug('');

shouldBe('checkRowNesting("tr")', '1');

debug('');

for (i = 0; i < otherTags.length; ++i)
    shouldBe('checkNoBodyRowNesting("' + otherTags[i] + '")', '0');

debug('');

for (i = 0; i < sectionTags.length; ++i)
    shouldBe('checkNoBodyRowNesting("' + sectionTags[i] + '")', '1');

debug('');

shouldBe('checkNoBodyRowNesting("tr")', '1');

debug('');

var successfullyParsed = true;
