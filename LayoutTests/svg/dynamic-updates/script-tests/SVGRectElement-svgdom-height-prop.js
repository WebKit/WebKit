// [Name] SVGRectElement-svgdom-height-prop.js
// [Expected rendering result] green rect size 200x200 at 0x0 - and a series of PASS messages

description("Tests dynamic updates of the 'height' property of the SVGRectElement object")
createSVGTestCase();

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "0");
rectElement.setAttribute("y", "0");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "50");
rectElement.setAttribute("fill", "green");

rootSVGElement.appendChild(rectElement);
shouldBe("rectElement.height.baseVal.value", "50");

function executeTest() {
    rectElement.height.baseVal.value = 200;
    shouldBe("rectElement.height.baseVal.value", "200");

    completeTest();
}

startTest(rectElement, 100, 25);

var successfullyParsed = true;
