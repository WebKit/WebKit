// [Name] SVGFEDiffuseLightingElement-dom-lighting-color-attr.js
// [Expected rendering result] A shining circle (performed by diffuse lighting) - and a series of PASS messages

description("Tests dynamic updates of the 'lighting-color' attribute of the SVGFEDiffuseLightingElement object")
createSVGTestCase();

var pointLight = createSVGElement("fePointLight");
pointLight.setAttribute("x", "100");
pointLight.setAttribute("y", "180");
pointLight.setAttribute("z", "30");

var blurElement = createSVGElement("feGaussianBlur");
blurElement.setAttribute("in", "SourceGraphic");
blurElement.setAttribute("stdDeviation", "2");
blurElement.setAttribute("result", "blur");

var gradientElement = createSVGElement("feDiffuseLighting");
gradientElement.setAttribute("in", "blur");
gradientElement.setAttribute("surfaceScale", "1");
gradientElement.setAttribute("diffuseConstant", "1");
gradientElement.setAttribute("lighting-color", "green");
gradientElement.appendChild(pointLight);

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(blurElement);
filterElement.appendChild(gradientElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);

var rectElement = createSVGElement("circle");
rectElement.setAttribute("width", 200);
rectElement.setAttribute("height", 200);
rectElement.setAttribute("cx", "100");
rectElement.setAttribute("cy", "60");
rectElement.setAttribute("r", "50");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("gradientElement.getAttribute('lighting-color')", "green");

function repaintTest() {
    gradientElement.setAttribute("lighting-color", "yellow");
    shouldBeEqualToString("gradientElement.getAttribute('lighting-color')", "yellow");

    completeTest();
}

var successfullyParsed = true;
