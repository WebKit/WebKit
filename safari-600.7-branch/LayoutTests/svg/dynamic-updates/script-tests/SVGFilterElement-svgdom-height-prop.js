// [Name] SVGFilterElement-svgdom-height-prop.js
// [Expected rendering result] An offseted gradient (performed by diffuse lighting) - and a series of PASS messages

description("Tests dynamic updates of the 'height' property of the SVGFilterElement object")
createSVGTestCase();

var pointLight = createSVGElement("fePointLight");
pointLight.setAttribute("x", "100");
pointLight.setAttribute("y", "100");
pointLight.setAttribute("z", "30");

var gradientElement = createSVGElement("feDiffuseLighting");
gradientElement.setAttribute("in", "SourceGraphic");
gradientElement.setAttribute("diffuseConstant", "1");
gradientElement.setAttribute("lighting-color", "yellow");
gradientElement.appendChild(pointLight);

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("x", "0");
filterElement.setAttribute("y", "0");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "100");
filterElement.appendChild(gradientElement);

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

shouldBe("filterElement.height.baseVal.value", "100");

function repaintTest() {
    filterElement.height.baseVal.value = 200;
    shouldBe("filterElement.height.baseVal.value", "200");

    completeTest();
}

var successfullyParsed = true;
