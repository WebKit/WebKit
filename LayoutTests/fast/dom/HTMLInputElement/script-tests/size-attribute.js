description("HTMLInputElement size attribute test");

function sizeAttributeEffect(value)
{
    var element = document.createElement("input");
    element.setAttribute("size", value);
    return element.size;
}

shouldBe('document.createElement("input").size', '20');

shouldBe('sizeAttributeEffect("")', '20');

shouldBe('sizeAttributeEffect("1")', '1');
shouldBe('sizeAttributeEffect("2")', '2');
shouldBe('sizeAttributeEffect("10")', '10');

shouldBe('sizeAttributeEffect("0")', '20');

shouldBe('sizeAttributeEffect("-1")', '20');

shouldBe('sizeAttributeEffect("1x")', '1');
shouldBe('sizeAttributeEffect("1.")', '1');
shouldBe('sizeAttributeEffect("1.9")', '1');
shouldBe('sizeAttributeEffect("2x")', '2');
shouldBe('sizeAttributeEffect("2.")', '2');
shouldBe('sizeAttributeEffect("2.9")', '2');

shouldBe('sizeAttributeEffect("a")', '20');

var arabicIndicDigitOne = String.fromCharCode(0x661);
shouldBe('sizeAttributeEffect(arabicIndicDigitOne)', '20');
shouldBe('sizeAttributeEffect("2" + arabicIndicDigitOne)', '2');
