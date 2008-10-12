// [Name] SVGImageElement-svgdom-width-prop.js
// [Expected rendering result] image at 0x0 size 200x200 - and a series of PASS messages

description("Tests dynamic updates of the 'width' property of the SVGImageElement object")
createSVGTestCase();

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../custom/resources/green-checker.png");
imageElement.setAttribute("preserveAspectRatio", "none");
imageElement.setAttribute("x", "0");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "100");
imageElement.setAttribute("height", "200");
rootSVGElement.appendChild(imageElement);

shouldBe("imageElement.width.baseVal.value", "100");

function executeTest() {
    imageElement.width.baseVal.value = 200;
    shouldBe("imageElement.width.baseVal.value", "200");

    completeTest();
}

startTest(imageElement, 50, 150);

var successfullyParsed = true;
