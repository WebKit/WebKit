// [Name] SVGImageElement-dom-preserveAspectRatio-attr.js
// [Expected rendering result] image at 0x0 size 100x200, needs to fit exactly in destination bbox (aka. preserveAspectRatio=none) - and a series of PASS messages

description("Tests dynamic updates of the 'preserveAspectRatio' attribute of the SVGImageElement object")
createSVGTestCase();

var imageElement = createSVGElement("image");
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../custom/resources/green-checker.png");
imageElement.setAttribute("preserveAspectRatio", "xMaxYMax meet");

imageElement.setAttribute("x", "0");
imageElement.setAttribute("y", "0");
imageElement.setAttribute("width", "100");
imageElement.setAttribute("height", "200");
rootSVGElement.appendChild(imageElement);

shouldBeEqualToString("imageElement.getAttribute('preserveAspectRatio')", "xMaxYMax meet");

function executeTest() {
    imageElement.setAttribute("preserveAspectRatio", "none");
    shouldBeEqualToString("imageElement.getAttribute('preserveAspectRatio')", "none");

    completeTest();
}

startTest(imageElement, 50, 150);

var successfullyParsed = true;
