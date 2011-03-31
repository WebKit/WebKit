// [Name] SVGFESpecularLightingElement-lighting-color-css-prop.js
// [Expected rendering result] A shining circle - and a series of PASS messages

description("Tests dynamic updates of the 'lighting-color' attribute of the SVGFESpecularLightingElement object")
createSVGTestCase();

var distantElement = createSVGElement("feDistantLight");
distantElement.setAttribute("azimuth", "45");
distantElement.setAttribute("elevation", "45");

var specularElement = createSVGElement("feSpecularLighting");
specularElement.setAttribute("in", "SourceGraphic");
specularElement.setAttribute("specularConstant", "1");
specularElement.setAttribute("specularExponent", "1");
specularElement.setAttribute("surfaceScale", "1");
specularElement.setAttribute("lighting-color", "rgb(255, 255, 0)");
specularElement.appendChild(distantElement);

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "objectBoundingBox");
filterElement.setAttribute("x", "0%");
filterElement.setAttribute("y", "0%");
filterElement.setAttribute("width", "100%");
filterElement.setAttribute("height", "100%");
filterElement.appendChild(specularElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);
rootSVGElement.setAttribute("width", "200");
rootSVGElement.setAttribute("height", "200");

var gElement = createSVGElement("g");
gElement.setAttribute("fill", "black");

var backgroundElement = createSVGElement("rect");
backgroundElement.setAttribute("x", 60);
backgroundElement.setAttribute("y", 20);
backgroundElement.setAttribute("width", 100);
backgroundElement.setAttribute("height", 60);
gElement.appendChild(backgroundElement);
rootSVGElement.appendChild(gElement);

var imageElement = createSVGElement("image");
imageElement.setAttribute("x", 60);
imageElement.setAttribute("y", 20);
imageElement.setAttribute("width", 100);
imageElement.setAttribute("height", 60);
imageElement.setAttributeNS(xlinkNS, "xlink:href", "../W3C-SVG-1.1/resources/bumpMap2.png");
imageElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(imageElement);

shouldBeEqualToString("document.defaultView.getComputedStyle(specularElement).getPropertyValue('lighting-color')", "rgb(255, 255, 0)");

function executeTest() {
    specularElement.style.setProperty("lighting-color", "greenyellow", "");
    shouldBeEqualToString("document.defaultView.getComputedStyle(specularElement).getPropertyValue('lighting-color')", "rgb(173, 255, 47)");

    completeTest();
}

startTest(rootSVGElement, 100, 100);

var successfullyParsed = true;
