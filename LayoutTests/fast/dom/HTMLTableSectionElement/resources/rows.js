description('Test behavior of the HTMLTableSectionElement rows attribute in cases where there is unusual nesting.');

function checkRowNesting(tag)
{
    var body = document.createElement("tbody");
    var container = document.createElement(tag);
    var row = document.createElement("tr");
    body.appendChild(container);
    container.appendChild(row);
    return body.rows.length;
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

var successfullyParsed = true;
