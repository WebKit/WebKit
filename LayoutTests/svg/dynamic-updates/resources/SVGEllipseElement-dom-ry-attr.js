// [Name] SVGEllipseElement-dom-ry-attr.js
// [Expected rendering result] green ellipse - and a series of PASS mesages

description("Tests dynamic updates of the 'ry' attribute of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "0");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBeEqualToString("ellipseElement.getAttribute('ry')", "0");

function executeTest() {
    ellipseElement.setAttribute("ry", "150");
    shouldBeEqualToString("ellipseElement.getAttribute('ry')", "150");

    waitForClickEvent(ellipseElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
