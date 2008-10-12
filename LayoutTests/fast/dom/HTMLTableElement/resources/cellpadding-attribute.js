description("HTMLTableElement cellpadding attribute test");

function cellPaddingAttributeEffect(value)
{
    var table = document.createElement("table");
    table.setAttribute("cellpadding", value);
    var cell = document.createElement("td");
    table.appendChild(cell);
    document.body.appendChild(table);
    var computedStyle = getComputedStyle(cell, "");
    var result = computedStyle.paddingTop;
    document.body.removeChild(table);
    return result;
}

shouldBe('cellPaddingAttributeEffect("")', '"1px"');

shouldBe('cellPaddingAttributeEffect("1")', '"1px"');
shouldBe('cellPaddingAttributeEffect("2")', '"2px"');
shouldBe('cellPaddingAttributeEffect("10")', '"10px"');

shouldBe('cellPaddingAttributeEffect("0")', '"0px"');

shouldBe('cellPaddingAttributeEffect("-1")', '"0px"');

shouldBe('cellPaddingAttributeEffect("1x")', '"1px"');
shouldBe('cellPaddingAttributeEffect("1.")', '"1px"');
shouldBe('cellPaddingAttributeEffect("1.9")', '"1px"');
shouldBe('cellPaddingAttributeEffect("2x")', '"2px"');
shouldBe('cellPaddingAttributeEffect("2.")', '"2px"');
shouldBe('cellPaddingAttributeEffect("2.9")', '"2px"');

shouldBe('cellPaddingAttributeEffect("a")', '"0px"');

var arabicIndicDigitOne = String.fromCharCode(0x661);
shouldBe('cellPaddingAttributeEffect(arabicIndicDigitOne)', '"0px"');
shouldBe('cellPaddingAttributeEffect("2" + arabicIndicDigitOne)', '"2px"');

var successfullyParsed = true;
