// [Name] SVGRectElement-svgdom-y-prop.js
// [Expected rendering result] green rect size 200x200 at 0x0 - and a series of PASS messages

description("Tests dynamic updates of the 'y' property of the SVGRectElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "0");
rectElement.setAttribute("y", "-150");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("fill", "green");

rootSVGElement.appendChild(rectElement);
shouldBe("rectElement.y.baseVal.value", "-150");

function executeTest() {
    rectElement.y.baseVal.value = 0;
    shouldBe("rectElement.y.baseVal.value", "0");

    completeTest();
}

startTest(rectElement, 100, 0);

var successfullyParsed = true;
