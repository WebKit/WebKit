// [Name] SVGCircleElement-svgdom-cy-prop.js
// [Expected rendering result] green circle - and a series of PASS mesages

description("Tests dynamic updates of the 'cy' property of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "150");
circleElement.setAttribute("cy", "-150");
circleElement.setAttribute("r", "150");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBe("circleElement.cy.baseVal.value", "-150");

function executeTest() {
    circleElement.cy.baseVal.value = "150";
    shouldBe("circleElement.cy.baseVal.value", "150");

    waitForClickEvent(circleElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
