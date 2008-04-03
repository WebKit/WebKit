// [Name] SVGEllipseElement-svgdom-cx-prop.js
// [Expected rendering result] green ellipse - and a series of PASS mesages

description("Tests dynamic updates of the 'cx' property of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "-150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBe("ellipseElement.cx.baseVal.value", "-150");

function executeTest() {
    ellipseElement.cx.baseVal.value = "150";
    shouldBe("ellipseElement.cx.baseVal.value", "150");

    waitForClickEvent(ellipseElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
