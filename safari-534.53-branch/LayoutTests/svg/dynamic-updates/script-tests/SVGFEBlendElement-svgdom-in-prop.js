// [Name] SVGFEBlendElement-svgdom-in-prop.js
// [Expected rendering result] Seven blended rectangles in a gradient - and a series of PASS messages

description("Tests dynamic updates of the 'in' property of the SVGFEBlendElement object")
createSVGTestCase();

var backgroundImage = createSVGElement("image");
backgroundImage.setAttribute("x", "35");
backgroundImage.setAttribute("y", "5");
backgroundImage.setAttribute("width", "220");
backgroundImage.setAttribute("height", "171");
backgroundImage.setAttribute("preserveAspectRatio", "none");
backgroundImage.setAttributeNS(xlinkNS, "xlink:href", "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAGQAAAABCAMAAAAfBfuPAAAABGdBTUEAAK/INwWK6QAAABl0RVh0         U29mdHdhcmUAQWRvYmUgSW1hZ2VSZWFkeXHJZTwAAAEsUExURfb/AK3/AAD/9/9sAIn/AN7/ABT/        AAB//zD/AP9GAAD/s1H/AAD/H/8AxwD/bn8A/1wA/wD/XJv/AP8Ai//MAGP/ABMA/wD/LP8A6P8K         AP8AF/8A9QD/xAAA/wD1/9MA/wD/gABY/wD/Cf8ATJEA//+6AACT/wCn/+v/AAD/TAD/kf8AYKMA         //8kAAAk/wAV/y0A/3b/AP80AAD/O/cA//8A/wD///8An/8A2QD/Ev8AAMQA/+0A/7MA////AP8A         CuAA//8AJ24A/0sA//8AOAA0/wAK/wBF//8WAAgA/x8A///2AL//AP8AdTwA/wn/AP/bAP+AAP+U         AM//AP+nAP8AtADp/wD/o0D/AP/qAADb/wC5/wDL/wD/AAD/7QD/0gD/4CH/AABr//9ZAG2IeB4A         AABvSURBVHjaYrCyl3J0Udb2FTYP5bWQkDY1sXGQt7bVUeF3EnD2M/AS85R109dzdVdMYldTj46J         CpMzY4pLiJfhCtdS4BPUNBbnsVSK9WdL5IjgFjVkEWL0CWbVYLDzjgwQUQ0JCmRO5jTS9ZAECDAA         3aQTV3E5iioAAAAASUVORK5CYII=");
rootSVGElement.appendChild(backgroundImage);

var normalFlood = createSVGElement("feFlood");
normalFlood.setAttribute("in", "BackgroundAlpha");
normalFlood.setAttribute("flood-color", "#0f0");
normalFlood.setAttribute("flood-opacity", "0.5");
normalFlood.setAttribute("result", "normalImg");

var multiplyFlood = createSVGElement("feFlood");
multiplyFlood.setAttribute("in", "SourceGraphic");
multiplyFlood.setAttribute("flood-color", "#0f0");
multiplyFlood.setAttribute("flood-opacity", "0.5");
multiplyFlood.setAttribute("result", "multiplyImg");

var screenFlood = createSVGElement("feFlood");
screenFlood.setAttribute("in", "SourceGraphic");
screenFlood.setAttribute("flood-color", "#0f0");
screenFlood.setAttribute("flood-opacity", "0.5");
screenFlood.setAttribute("result", "screenImg");

var darkenFlood = createSVGElement("feFlood");
darkenFlood.setAttribute("in", "SourceGraphic");
darkenFlood.setAttribute("flood-color", "#0f0");
darkenFlood.setAttribute("flood-opacity", "0.5");
darkenFlood.setAttribute("result", "darkenImg");

var lightenFlood = createSVGElement("feFlood");
lightenFlood.setAttribute("in", "SourceGraphic");
lightenFlood.setAttribute("flood-color", "#0f0");
lightenFlood.setAttribute("flood-opacity", "0.5");
lightenFlood.setAttribute("result", "lightenImg");

var normalBlend = createSVGElement("feBlend");
normalBlend.setAttribute("in", "SourceAlpha");
normalBlend.setAttribute("in2", "normalImg");
normalBlend.setAttribute("mode", "normal");

var multiplyBlend = createSVGElement("feBlend");
multiplyBlend.setAttribute("in", "SourceAlpha");
multiplyBlend.setAttribute("in2", "multiplyImg");
multiplyBlend.setAttribute("mode", "multiply");

var screenBlend = createSVGElement("feBlend");
screenBlend.setAttribute("in", "SourceAlpha");
screenBlend.setAttribute("in2", "screenImg");
screenBlend.setAttribute("mode", "screen");

var darkenBlend = createSVGElement("feBlend");
darkenBlend.setAttribute("in", "SourceAlpha");
darkenBlend.setAttribute("in2", "darkenImg");
darkenBlend.setAttribute("mode", "darken");

var lightenBlend = createSVGElement("feBlend");
lightenBlend.setAttribute("in", "SourceAlpha");
lightenBlend.setAttribute("in2", "lightenImg");
lightenBlend.setAttribute("mode", "lighten");

var normalBlendFilter = createSVGElement("filter");
normalBlendFilter.setAttribute("id", "normalFilter");
normalBlendFilter.setAttribute("filterUnits", "objectBoundingBox");
normalBlendFilter.setAttribute("x", "0%");
normalBlendFilter.setAttribute("y", "0%");
normalBlendFilter.setAttribute("width", "100%");
normalBlendFilter.setAttribute("height", "100%");
normalBlendFilter.appendChild(normalFlood);
normalBlendFilter.appendChild(normalBlend);

var multiplyBlendFilter = createSVGElement("filter");
multiplyBlendFilter.setAttribute("id", "multiplyFilter");
multiplyBlendFilter.setAttribute("filterUnits", "objectBoundingBox");
multiplyBlendFilter.setAttribute("x", "0%");
multiplyBlendFilter.setAttribute("y", "0%");
multiplyBlendFilter.setAttribute("width", "100%");
multiplyBlendFilter.setAttribute("height", "100%");
multiplyBlendFilter.appendChild(multiplyFlood);
multiplyBlendFilter.appendChild(multiplyBlend);

var screenBlendFilter = createSVGElement("filter");
screenBlendFilter.setAttribute("id", "screenFilter");
screenBlendFilter.setAttribute("filterUnits", "objectBoundingBox");
screenBlendFilter.setAttribute("x", "0%");
screenBlendFilter.setAttribute("y", "0%");
screenBlendFilter.setAttribute("width", "100%");
screenBlendFilter.setAttribute("height", "100%");
screenBlendFilter.appendChild(screenFlood);
screenBlendFilter.appendChild(screenBlend);

var darkenBlendFilter = createSVGElement("filter");
darkenBlendFilter.setAttribute("id", "darkenFilter");
darkenBlendFilter.setAttribute("filterUnits", "objectBoundingBox");
darkenBlendFilter.setAttribute("x", "0%");
darkenBlendFilter.setAttribute("y", "0%");
darkenBlendFilter.setAttribute("width", "100%");
darkenBlendFilter.setAttribute("height", "100%");
darkenBlendFilter.appendChild(darkenFlood);
darkenBlendFilter.appendChild(darkenBlend);

var lightenBlendFilter = createSVGElement("filter");
lightenBlendFilter.setAttribute("id", "lightenFilter");
lightenBlendFilter.setAttribute("filterUnits", "objectBoundingBox");
lightenBlendFilter.setAttribute("x", "0%");
lightenBlendFilter.setAttribute("y", "0%");
lightenBlendFilter.setAttribute("width", "100%");
lightenBlendFilter.setAttribute("height", "100%");
lightenBlendFilter.appendChild(lightenFlood);
lightenBlendFilter.appendChild(lightenBlend);

var defsElement = createSVGElement("defs");
defsElement.appendChild(normalBlendFilter);
defsElement.appendChild(multiplyBlendFilter);
defsElement.appendChild(screenBlendFilter);
defsElement.appendChild(darkenBlendFilter);
defsElement.appendChild(lightenBlendFilter);

rootSVGElement.appendChild(defsElement);

var normalRectElement = createSVGElement("rect");
normalRectElement.setAttribute("x", "25");
normalRectElement.setAttribute("y", "10");
normalRectElement.setAttribute("width", "240");
normalRectElement.setAttribute("height", "20");
normalRectElement.setAttribute("fill", "blue");
normalRectElement.setAttribute("opacity", "0.5");
normalRectElement.setAttribute("filter", "url(#normalFilter)");
rootSVGElement.appendChild(normalRectElement);

var multiplyRectElement = createSVGElement("rect");
multiplyRectElement.setAttribute("x", "25");
multiplyRectElement.setAttribute("y", "33");
multiplyRectElement.setAttribute("width", "240");
multiplyRectElement.setAttribute("height", "20");
multiplyRectElement.setAttribute("fill", "blue");
multiplyRectElement.setAttribute("opacity", "0.5");
multiplyRectElement.setAttribute("filter", "url(#multiplyFilter)");
rootSVGElement.appendChild(multiplyRectElement);

var gElement = createSVGElement("g");
gElement.setAttribute("filter", "url(#multiplyFilter)");

var embeddedMultiplyRectElement1 = createSVGElement("rect");
embeddedMultiplyRectElement1.setAttribute("x", "25");
embeddedMultiplyRectElement1.setAttribute("y", "56");
embeddedMultiplyRectElement1.setAttribute("width", "240");
embeddedMultiplyRectElement1.setAttribute("height", "20");
embeddedMultiplyRectElement1.setAttribute("fill", "blue");
embeddedMultiplyRectElement1.setAttribute("opacity", "0.5");
gElement.appendChild(embeddedMultiplyRectElement1);

var embeddedMultiplyRectElement2 = createSVGElement("rect");
embeddedMultiplyRectElement2.setAttribute("x", "25");
embeddedMultiplyRectElement2.setAttribute("y", "79");
embeddedMultiplyRectElement2.setAttribute("width", "240");
embeddedMultiplyRectElement2.setAttribute("height", "20");
embeddedMultiplyRectElement2.setAttribute("fill", "#ff0");
embeddedMultiplyRectElement2.setAttribute("opacity", "0.5");
gElement.appendChild(embeddedMultiplyRectElement2);

rootSVGElement.appendChild(gElement);

var screenRectElement = createSVGElement("rect");
screenRectElement.setAttribute("x", "25");
screenRectElement.setAttribute("y", "102");
screenRectElement.setAttribute("width", "240");
screenRectElement.setAttribute("height", "20");
screenRectElement.setAttribute("fill", "blue");
screenRectElement.setAttribute("opacity", "0.5");
screenRectElement.setAttribute("filter", "url(#screenFilter)");
rootSVGElement.appendChild(screenRectElement);

var darkenRectElement = createSVGElement("rect");
darkenRectElement.setAttribute("x", "25");
darkenRectElement.setAttribute("y", "125");
darkenRectElement.setAttribute("width", "240");
darkenRectElement.setAttribute("height", "20");
darkenRectElement.setAttribute("fill", "blue");
darkenRectElement.setAttribute("opacity", "0.5");
darkenRectElement.setAttribute("filter", "url(#darkenFilter)");
rootSVGElement.appendChild(darkenRectElement);

var lightenRectElement = createSVGElement("rect");
lightenRectElement.setAttribute("x", "25");
lightenRectElement.setAttribute("y", "148");
lightenRectElement.setAttribute("width", "240");
lightenRectElement.setAttribute("height", "20");
lightenRectElement.setAttribute("fill", "blue");
lightenRectElement.setAttribute("opacity", "0.5");
lightenRectElement.setAttribute("filter", "url(#lightenFilter)");
rootSVGElement.appendChild(lightenRectElement);

rootSVGElement.setAttribute("fill", "#333");
rootSVGElement.setAttribute("font-size", "14");
rootSVGElement.setAttribute("width", "350");
rootSVGElement.setAttribute("height", "250");

shouldBeEqualToString("normalBlend.in1.baseVal", "SourceAlpha");
shouldBeEqualToString("multiplyBlend.in1.baseVal", "SourceAlpha");
shouldBeEqualToString("screenBlend.in1.baseVal", "SourceAlpha");
shouldBeEqualToString("darkenBlend.in1.baseVal", "SourceAlpha");
shouldBeEqualToString("lightenBlend.in1.baseVal", "SourceAlpha");

function executeTest() {
    normalBlend.in1.baseVal = "SourceGraphic";
    multiplyBlend.in1.baseVal = "SourceGraphic";
    screenBlend.in1.baseVal = "SourceGraphic";
    darkenBlend.in1.baseVal = "SourceGraphic";
    lightenBlend.in1.baseVal = "SourceGraphic";

    shouldBeEqualToString("normalBlend.in1.baseVal", "SourceGraphic");
    shouldBeEqualToString("multiplyBlend.in1.baseVal", "SourceGraphic");
    shouldBeEqualToString("screenBlend.in1.baseVal", "SourceGraphic");
    shouldBeEqualToString("darkenBlend.in1.baseVal", "SourceGraphic");
    shouldBeEqualToString("lightenBlend.in1.baseVal", "SourceGraphic");

    completeTest();
}

startTest(rootSVGElement, 100, 100);

var successfullyParsed = true;
