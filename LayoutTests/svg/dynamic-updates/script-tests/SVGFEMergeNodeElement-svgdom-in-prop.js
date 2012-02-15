// [Name] SVGFEMergeNodeElement-svgdom-in-prop.js
// [Expected rendering result] Two circle merged with feMerge filter - and a series of PASS messages

description("Tests dynamic updates of the 'in' property of the SVGFEMergeNodeElement object")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var off = createSVGElement("feOffset");
off.setAttribute("dx", "105");
off.setAttribute("dy", "25");
off.setAttribute("result", "off");

var merge = createSVGElement("feMerge");

var mergeNode1 = createSVGElement("feMergeNode");
mergeNode1.setAttribute("in", "SourceGraphic");

var mergeNode2 = createSVGElement("feMergeNode");
mergeNode2.setAttribute("in", "SourceGraphic");

merge.appendChild(mergeNode1);
merge.appendChild(mergeNode2);

var mergeFilter = createSVGElement("filter");
mergeFilter.setAttribute("id", "myFilter");
mergeFilter.setAttribute("filterUnits", "objectBoundingBox");
mergeFilter.setAttribute("x", "0");
mergeFilter.setAttribute("y", "0");
mergeFilter.setAttribute("width", "3.5");
mergeFilter.setAttribute("height", "4");
mergeFilter.appendChild(off);
mergeFilter.appendChild(merge);

defsElement.appendChild(mergeFilter);

var myCircle = createSVGElement("circle");
myCircle.setAttribute("cx", "100");
myCircle.setAttribute("cy", "50");
myCircle.setAttribute("r", "50");
myCircle.setAttribute("fill", "#408067");
myCircle.setAttribute("filter", "url(#myFilter)");

rootSVGElement.setAttribute("height", "200");
rootSVGElement.appendChild(myCircle);

shouldBeEqualToString("mergeNode1.in1.baseVal", "SourceGraphic");

function repaintTest() {
    mergeNode1.in1.baseVal = "off";
    shouldBeEqualToString("mergeNode1.in1.baseVal", "off");

    completeTest();
}

var successfullyParsed = true;
