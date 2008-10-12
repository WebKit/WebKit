// [Name] SVGCircleElement-svgdom-cx-prop.js
// [Expected rendering result] unclipped green circle - and a series of PASS messages

description("Tests dynamic updates of the 'cx' property of the SVGCircleElement object")
createSVGTestCase();

var circleElement = createSVGElement("circle");
circleElement.setAttribute("cx", "-50");
circleElement.setAttribute("cy", "150");
circleElement.setAttribute("r", "150");
circleElement.setAttribute("fill", "green");

rootSVGElement.appendChild(circleElement);
shouldBe("circleElement.cx.baseVal.value", "-50");

function executeTest() {
    circleElement.cx.baseVal.value = "150";
    shouldBe("circleElement.cx.baseVal.value", "150");

    completeTest();
}

startTest(circleElement, 50, 150);

var successfullyParsed = true;
