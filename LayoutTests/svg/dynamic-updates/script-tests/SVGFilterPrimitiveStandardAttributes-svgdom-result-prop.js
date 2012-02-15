// [Name] SVGFilterPrimitiveStandardAttributes-svgdom-result-prop.js
// [Expected rendering result] An offseted gradient (performed by diffuse lighting) - and a series of PASS messages

description("Tests dynamic updates of the 'result' propert of the SVGFilterPrimitiveStandardAttributes object")
createSVGTestCase();

var pointLight = createSVGElement("fePointLight");
pointLight.setAttribute("x", "100");
pointLight.setAttribute("y", "100");
pointLight.setAttribute("z", "30");

var gradientElement = createSVGElement("feDiffuseLighting");
gradientElement.setAttribute("in", "SourceGraphic");
gradientElement.setAttribute("diffuseConstant", "1");
gradientElement.setAttribute("lighting-color", "yellow");
gradientElement.setAttribute("x", "0");
gradientElement.setAttribute("y", "0");
gradientElement.setAttribute("width", "200");
gradientElement.setAttribute("height", "200");
gradientElement.setAttribute("result", "res1");
gradientElement.appendChild(pointLight);

var offsetElement = createSVGElement("feOffset");
offsetElement.setAttribute("in", "res2");

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(gradientElement);
filterElement.appendChild(offsetElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);

var rectElement = createSVGElement("rect");
rectElement.setAttribute("x", "0");
rectElement.setAttribute("y", "0");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBeEqualToString("gradientElement.result.baseVal", "res1");

function repaintTest() {
    gradientElement.result.baseVal = "res2";
    shouldBeEqualToString("gradientElement.result.baseVal", "res2");

    completeTest();
}

var successfullyParsed = true;
