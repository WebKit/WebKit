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
shouldBeFalse('createRegionElement("div", "compact")');
shouldBeFalse('createRegionElement("div", "inline")');
shouldBeFalse('createRegionElement("div", "table")');
shouldBeFalse('createRegionElement("div", "inline-table")');
shouldBeTrue('createRegionElement("div", "table-cell")');
shouldBeTrue('createRegionElement("div", "table-caption")');
shouldBeTrue('createRegionElement("div", "list-item")');
shouldBeFalse('createRegionElement("div", "-webkit-box")');
shouldBeFalse('createRegionElement("div", "-webkit-inline-box")');
shouldBeFalse('createRegionElement("div", "-webkit-flex")');
shouldBeFalse('createRegionElement("div", "-webkit-inline-flex")');
// FIXME: Also add grid and inline grid when it is enabled by default
