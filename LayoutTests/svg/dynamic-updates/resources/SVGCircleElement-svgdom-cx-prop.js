// [Name] SVGCircleElement-svgdom-cx-prop.js
// [Expected rendering result] green circle - and a series of PASS mesages

description("Tests dynamic updates of the 'cx' property of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "-150");
circleElement.setAttribute("cy", "150");
circleElement.setAttribute("r", "150");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBe("circleElement.cx.baseVal.value", "-150");

function executeTest() {
    circleElement.cx.baseVal.value = "150";
    shouldBe("circleElement.cx.baseVal.value", "150");

    waitForClickEvent(circleElement);
    triggerUpdate();
}

executeTest();
var successfullyParsed = true;
