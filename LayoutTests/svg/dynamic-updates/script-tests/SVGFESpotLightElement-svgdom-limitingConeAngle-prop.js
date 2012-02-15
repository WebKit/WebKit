// [Name] SVGFESpotLightElement-svgdom-limitingConeAngle-prop.js
// [Expected rendering result] A shining cone (performed by diffuse lighting) - and a series of PASS messages

description("Tests dynamic updates of the 'limitingConeAngle' property of the SVGFESpotLightElement object")
createSVGTestCase();

var spotLightElement = createSVGElement("feSpotLight");
spotLightElement.setAttribute("x", "113");
spotLightElement.setAttribute("y", "0");
spotLightElement.setAttribute("z", "100");
spotLightElement.setAttribute("pointsAtX", "113");
spotLightElement.setAttribute("pointsAtY", "100");
spotLightElement.setAttribute("pointsAtZ", "0");
spotLightElement.setAttribute("specularExponent", "1");
spotLightElement.setAttribute("limitingConeAngle", "45");

var gradientElement = createSVGElement("feDiffuseLighting");
gradientElement.setAttribute("surfaceScale", "1");
gradientElement.setAttribute("diffuseConstant", "1");
gradientElement.setAttribute("lighting-color", "aqua");
gradientElement.appendChild(spotLightElement);

var filterElement = createSVGElement("filter");
filterElement.setAttribute("id", "myFilter");
filterElement.setAttribute("filterUnits", "userSpaceOnUse");
filterElement.setAttribute("width", "200");
filterElement.setAttribute("height", "200");
filterElement.appendChild(gradientElement);

var defsElement = createSVGElement("defs");
defsElement.appendChild(filterElement);

rootSVGElement.appendChild(defsElement);

var rectElement = createSVGElement("rect");
rectElement.setAttribute("width", "200");
rectElement.setAttribute("height", "200");
rectElement.setAttribute("filter", "url(#myFilter)");
rootSVGElement.appendChild(rectElement);

shouldBe("spotLightElement.limitingConeAngle.baseVal", "45");

function repaintTest() {
    spotLightElement.limitingConeAngle.baseVal = 15;
    shouldBe("spotLightElement.limitingConeAngle.baseVal", "15");

    completeTest();
}


var successfullyParsed = true;
