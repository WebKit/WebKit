// [Name] SVGEllipseElement-svgdom-cy-prop.js
// [Expected rendering result] green ellipse - and a series of PASS mesages

description("Tests dynamic updates of the 'cy' property of the SVGEllipseElement object")
createSVGTestCase();

var ellipseElement = createSVGElement("ellipse");
ellipseElement.setAttribute("cx", "150");
ellipseElement.setAttribute("cy", "-150");
ellipseElement.setAttribute("rx", "100");
ellipseElement.setAttribute("ry", "150");
ellipseElement.setAttribute("fill", "green");

rootSVGElement.appendChild(ellipseElement);
shouldBe("ellipseElement.cy.baseVal.value", "-150");

function executeTest() {
    ellipseElement.cy.baseVal.value = "150";
    shouldBe("ellipseElement.cy.baseVal.value", "150");

    waitForClickEvent(ellipseElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
