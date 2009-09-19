// [Name] SVGCircleElement-svgdom-cy-prop.js
// [Expected rendering result] unclipped green circle - and a series of PASS messages

description("Tests dynamic updates of the 'cy' property of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "-50");
circleElement.setAttribute("r", "150");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBe("circleElement.cy.baseVal.value", "-50");

function executeTest() {
    circleElement.cy.baseVal.value = "150";
    shouldBe("circleElement.cy.baseVal.value", "150");

    completeTest();
}

startTest(circleElement, 150, 50);

var successfullyParsed = true;
