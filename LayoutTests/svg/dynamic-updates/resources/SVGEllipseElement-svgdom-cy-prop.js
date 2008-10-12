// [Name] SVGEllipseElement-svgdom-cy-prop.js
// [Expected rendering result] unclipped green ellipse - and a series of PASS messages

description("Tests dynamic updates of the 'cy' property of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "-50");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBe("ellipseElement.cy.baseVal.value", "-50");

function executeTest() {
    ellipseElement.cy.baseVal.value = "150";
    shouldBe("ellipseElement.cy.baseVal.value", "150");

    completeTest();
}

startTest(ellipseElement, 150, 50);

var successfullyParsed = true;
