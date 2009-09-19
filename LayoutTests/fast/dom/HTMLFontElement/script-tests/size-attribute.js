description("HTMLFontElement size attribute test");

function fontSizeAttributeEffect(value)
{
    var element = document.createElement("font");
    element.setAttribute("size", value);
    var outerElement = document.createElement("p");
    outerElement.setAttribute("style", "font-size: 100px");
    document.body.appendChild(outerElement);
    outerElement.appendChild(element);
    var computedStyle = getComputedStyle(element, "");
    var result = computedStyle.fontSize;
    document.body.removeChild(outerElement);
    return result === "100px" ? null : result;
}

shouldBe('fontSizeAttributeEffect("")', 'null');

shouldBe('fontSizeAttributeEffect("1")', '"10px"');
shouldBe('fontSizeAttributeEffect("2")', '"13px"');
shouldBe('fontSizeAttributeEffect("3")', '"16px"');
shouldBe('fontSizeAttributeEffect("4")', '"18px"');
shouldBe('fontSizeAttributeEffect("5")', '"24px"');
shouldBe('fontSizeAttributeEffect("6")', '"32px"');
shouldBe('fontSizeAttributeEffect("7")', '"48px"');

shouldBe('fontSizeAttributeEffect("0")', '"16px"'); // Gecko and WebKit do not agree on this result. Which matches IE?

shouldBe('fontSizeAttributeEffect("-1")', '"13px"');
shouldBe('fontSizeAttributeEffect("-2")', '"10px"');
shouldBe('fontSizeAttributeEffect("-3")', '"10px"');
shouldBe('fontSizeAttributeEffect("-4")', '"10px"');
shouldBe('fontSizeAttributeEffect("-5")', '"10px"');
shouldBe('fontSizeAttributeEffect("-6")', '"10px"');
shouldBe('fontSizeAttributeEffect("-7")', '"10px"');
shouldBe('fontSizeAttributeEffect("-8")', '"10px"');
shouldBe('fontSizeAttributeEffect("-9")', '"10px"');
shouldBe('fontSizeAttributeEffect("-10")', '"10px"');

shouldBe('fontSizeAttributeEffect("8")', '"48px"');
shouldBe('fontSizeAttributeEffect("9")', '"48px"');
shouldBe('fontSizeAttributeEffect("10")', '"48px"');
shouldBe('fontSizeAttributeEffect("100")', '"48px"');
shouldBe('fontSizeAttributeEffect("1000")', '"48px"');

shouldBe('fontSizeAttributeEffect("1x")', '"10px"');
shouldBe('fontSizeAttributeEffect("1.")', '"10px"');
shouldBe('fontSizeAttributeEffect("1.9")', '"10px"');
shouldBe('fontSizeAttributeEffect("2x")', '"13px"');
shouldBe('fontSizeAttributeEffect("2.")', '"13px"');
shouldBe('fontSizeAttributeEffect("2.9")', '"13px"');

shouldBe('fontSizeAttributeEffect("a")', 'null');

var arabicIndicDigitOne = String.fromCharCode(0x661);
shouldBe('fontSizeAttributeEffect(arabicIndicDigitOne)', 'null');

var successfullyParsed = true;
