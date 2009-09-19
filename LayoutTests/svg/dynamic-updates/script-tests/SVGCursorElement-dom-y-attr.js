// [Name] SVGCursorElement-dom-y-attr.js
// [Expected rendering result] verify manually that the cursor is visible when hovering the rect - and a series of PASS messages

description("Tests dynamic updates of the 'y' attribute of the SVGCursorElement object")
createSVGTestCase();

var cursorElement = createSVGElement("cursor");
cursorElement.setAttribute("id", "cursor");
cursorElement.setAttribute("y", "500");
cursorElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/magnify.png");
rootSVGElement.appendChild(cursorElement);

var rectElement = createSVGElement("rect");
rectElement.setAttribute("cursor", "url(#cursor)");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("fill", "green");
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("cursorElement.getAttribute('y')", "500");

function executeTest() {
    cursorElement.setAttribute("y", "0");
    shouldBeEqualToString("cursorElement.getAttribute('y')", "0");

    completeTest();
}

startTest(rectElement, 150, 150);

var successfullyParsed = true;
