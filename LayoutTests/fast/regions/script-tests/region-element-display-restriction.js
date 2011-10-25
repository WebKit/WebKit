description("Test that only non-replaced block elements can be transformed into regions.");

function createRegionElement(elementType, displayType)
{
    var element = document.createElement(elementType);
    var textElement = document.createTextNode("inside element");
    element.appendChild(textElement);
    element.style.setProperty("display", displayType);
    document.body.appendChild(element);

    // Transform the element into a region.
    element.style.setProperty("-webkit-flow-from", "no-flow");

    // The region element was created if the length of inner text is 0.
    var regionCreated = element.innerText.length == 0;

    document.body.removeChild(element);

    return regionCreated;
}

shouldBeFalse('createRegionElement("div", "none")');
shouldBeTrue('createRegionElement("div", "block")');
shouldBeTrue('createRegionElement("div", "inline-block")');
shouldBeTrue('createRegionElement("div", "run-in")');
shouldBeTrue('createRegionElement("div", "compact")');
shouldBeFalse('createRegionElement("div", "inline")');
shouldBeFalse('createRegionElement("div", "table")');
shouldBeFalse('createRegionElement("div", "inline-table")');
