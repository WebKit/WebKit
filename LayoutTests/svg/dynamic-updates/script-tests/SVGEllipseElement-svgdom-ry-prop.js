// [Name] SVGEllipseElement-svgdom-ry-prop.js
// [Expected rendering result] green ellipse with rx = ry (in fact a circle)- and a series of PASS messages

description("Tests dynamic updates of the 'ry' property of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "150");
ellipseElement.setAttribute("rx", "150");
ellipseElement.setAttribute("ry", "10");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBe("ellipseElement.ry.baseVal.value", "10");

function executeTest() {
    ellipseElement.ry.baseVal.value = "150";
    shouldBe("ellipseElement.ry.baseVal.value", "150");

    completeTest();
}

startTest(ellipseElement, 150, 150);

var successfullyParsed = true;
