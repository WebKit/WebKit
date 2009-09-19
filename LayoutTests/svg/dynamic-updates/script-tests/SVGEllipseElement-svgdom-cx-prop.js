// [Name] SVGEllipseElement-svgdom-cx-prop.js
// [Expected rendering result] unclipped green ellipse - and a series of PASS messages

description("Tests dynamic updates of the 'cx' property of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "-50");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBe("ellipseElement.cx.baseVal.value", "-50");

function executeTest() {
    ellipseElement.cx.baseVal.value = "150";
    shouldBe("ellipseElement.cx.baseVal.value", "150");

    completeTest();
}

startTest(ellipseElement, 45, 150);

var successfullyParsed = true;
