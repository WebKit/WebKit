// [Name] SVGCursorElement-dom-y-attr.js
// [Expected rendering result] cursor image should hang from the upper-left corner of the cursor after clicking - and a series of PASS mesasges

description("Tests dynamic updates of the 'y' attribute of the SVGCursorElement object")
createSVGTestCase();

var cursorElement = createSVGElement("cursor");
cursorElement.setAttribute("id", "cursor");
cursorElement.setAttribute("y", "100");
cursorElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/sphere.png");
rootSVGElement.appendChild(cursorElement);

var rectElement = createSVGElement("rect");
rectElement.setAttribute("cursor", "url(#cursor)");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("fill", "green");
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("cursorElement.getAttribute('y')", "100");

function repaintTest() {
    cursorElement.setAttribute("y", "0");
    shouldBeEqualToString("cursorElement.getAttribute('y')", "0");

    completeTest();
}

var successfullyParsed = true;
