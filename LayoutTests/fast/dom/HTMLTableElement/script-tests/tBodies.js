description('Test behavior of the HTMLTableElement tBodies attribute in cases where there is unusual nesting.');

function checkTBodyNesting(tag)
{
    var table = document.createElement("table");
    var container = document.createElement(tag);
    var tbody = document.createElement("tbody");
    table.appendChild(container);
    container.appendChild(tbody);
    return table.tBodies.length;
}

var tags = [
    "col",
    "colgroup",
    "div",
    "form",
    "script",
    "table",
    "td",
    "tfoot",
    "th",
    "thead",
    "tr",
];

for (i = 0; i < tags.length; ++i)
    shouldBe('checkTBodyNesting("' + tags[i] + '")', '0');

debug('');

shouldBe('checkTBodyNesting("tbody")', '1');

debug('');

var successfullyParsed = true;
