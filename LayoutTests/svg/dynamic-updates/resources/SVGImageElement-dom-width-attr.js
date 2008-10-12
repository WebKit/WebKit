// [Name] SVGImageElement-dom-width-attr.js
// [Expected rendering result] image at 0x0 size 200x200 - and a series of PASS messages

description("Tests dynamic updates of the 'width' attribute of the SVGImageElement object")
createSVGTestCase();

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../custom/resources/green-checker.png");
imageElement.setAttribute("preserveAspectRatio", "none");
imageElement.setAttribute("x", "0");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "100");
imageElement.setAttribute("height", "200");
rootSVGElement.appendChild(imageElement);

shouldBeEqualToString("imageElement.getAttribute('width')", "100");

function executeTest() {
    imageElement.setAttribute("width", "200");
    shouldBeEqualToString("imageElement.getAttribute('width')", "200");

    completeTest();
}

startTest(imageElement, 50, 150);

var successfullyParsed = true;
