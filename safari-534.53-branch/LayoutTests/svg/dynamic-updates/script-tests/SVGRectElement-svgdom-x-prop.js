// [Name] SVGRectElement-svgdom-x-prop.js
// [Expected rendering result] green rect size 200x200 at 0x0 - and a series of PASS messages

description("Tests dynamic updates of the 'x' property of the SVGRectElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "-150");
rectElement.setAttribute("y", "0");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("fill", "green");

rootSVGElement.appendChild(rectElement);
shouldBe("rectElement.x.baseVal.value", "-150");

function executeTest() {
    rectElement.x.baseVal.value = 0;
    shouldBe("rectElement.x.baseVal.value", "0");

    completeTest();
}

startTest(rectElement, 0, 100);

var successfullyParsed = true;
