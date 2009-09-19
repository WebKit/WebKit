// [Name] SVGImageElement-dom-height-attr.js
// [Expected rendering result] image at 0x0 size 200x200 - and a series of PASS messages

description("Tests dynamic updates of the 'height' attribute of the SVGImageElement object")
createSVGTestCase();

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../custom/resources/green-checker.png");
imageElement.setAttribute("preserveAspectRatio", "none");
imageElement.setAttribute("x", "0");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "200");
imageElement.setAttribute("height", "100");
rootSVGElement.appendChild(imageElement);

shouldBeEqualToString("imageElement.getAttribute('height')", "100");

function executeTest() {
    imageElement.setAttribute("height", "200");
    shouldBeEqualToString("imageElement.getAttribute('height')", "200");

    completeTest();
}

startTest(imageElement, 150, 50);

var successfullyParsed = true;
