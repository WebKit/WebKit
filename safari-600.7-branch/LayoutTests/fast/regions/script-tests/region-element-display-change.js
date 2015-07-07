description("Test that RenderObject is recreated correctly after changing display type.");

function testElement(element, displayType)
{
	element.style.setProperty("display", displayType);
	// The region element was created if the length of inner text is 0.
    var regionCreated = element.innerText.length == 0;
	return regionCreated;
}

var element = document.createElement("div");
var textElement = document.createTextNode("inside element");
element.appendChild(textElement);
element.style.setProperty("-webkit-flow-from", "flow");
document.body.appendChild(element);
shouldBeFalse('testElement(element, "none")');
shouldBeTrue('testElement(element, "block")');
shouldBeTrue('testElement(element, "inline-block")');
shouldBeFalse('testElement(element, "compact")');
shouldBeFalse('testElement(element, "inline")');
shouldBeFalse('testElement(element, "table")');
shouldBeFalse('testElement(element, "inline-table")');
shouldBeTrue('testElement(element, "table-cell")');
shouldBeTrue('testElement(element, "table-caption")');
shouldBeTrue('testElement(element, "list-item")');
shouldBeFalse('testElement(element, "-webkit-box")');
shouldBeFalse('testElement(element, "-webkit-inline-box")');
shouldBeFalse('testElement(element, "-webkit-flex")');
shouldBeFalse('testElement(element, "-webkit-inline-flex")');
// FIXME: Also add grid and inline grid when it is enabled by default
document.body.removeChild(element);
