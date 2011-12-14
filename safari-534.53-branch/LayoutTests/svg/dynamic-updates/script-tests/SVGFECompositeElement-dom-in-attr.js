// [Name] SVGFECompositeElement-dom-in-attr.js
// [Expected rendering result] Four circle with different opacity merged with feComposite filter - and a series of PASS messages

description("Tests dynamic updates of the 'in' attribute of the SVGFECompositeElement object")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var off1 = createSVGElement("feOffset");
off1.setAttribute("dx", "35");
off1.setAttribute("dy", "25");
off1.setAttribute("result", "off1");

var flood1 = createSVGElement("feFlood");
flood1.setAttribute("flood-color", "#408067");
flood1.setAttribute("flood-opacity", ".8");
flood1.setAttribute("result", "F1");

var flood12 = createSVGElement("feFlood");
flood12.setAttribute("flood-color", "#008097");
flood12.setAttribute("flood-opacity", ".9");
flood12.setAttribute("result", "F12");

var overComposite1 = createSVGElement("feComposite");
overComposite1.setAttribute("in", "F12");
overComposite1.setAttribute("in2", "off1");
overComposite1.setAttribute("operator", "in");
overComposite1.setAttribute("result", "C1");

var off2 = createSVGElement("feOffset");
off2.setAttribute("in", "SourceGraphic");
off2.setAttribute("dx", "60");
off2.setAttribute("dy", "50");
off2.setAttribute("result", "off2");

var flood2 = createSVGElement("feFlood");
flood2.setAttribute("flood-color", "#408067");
flood2.setAttribute("flood-opacity", ".6");
flood2.setAttribute("result", "F2");

var overComposite2 = createSVGElement("feComposite");
overComposite2.setAttribute("in", "F2");
overComposite2.setAttribute("in2", "off2");
overComposite2.setAttribute("operator", "in");
overComposite2.setAttribute("result", "C2");

var off3 = createSVGElement("feOffset");
off3.setAttribute("in", "SourceGraphic");
off3.setAttribute("dx", "85");
off3.setAttribute("dy", "75");
off3.setAttribute("result", "off3");

var flood3 = createSVGElement("feFlood");
flood3.setAttribute("flood-color", "#408067");
flood3.setAttribute("flood-opacity", ".4");
flood3.setAttribute("result", "F3");

var overComposite3 = createSVGElement("feComposite");
overComposite3.setAttribute("in2", "off3");
overComposite3.setAttribute("operator", "in");
overComposite3.setAttribute("result", "C3");

var merge = createSVGElement("feMerge");

var mergeNode1 = createSVGElement("feMergeNode");
mergeNode1.setAttribute("in", "C1");

var mergeNode2 = createSVGElement("feMergeNode");
mergeNode2.setAttribute("in", "C2");

var mergeNode3 = createSVGElement("feMergeNode");
mergeNode3.setAttribute("in", "C3");

var mergeNode4 = createSVGElement("feMergeNode");
mergeNode4.setAttribute("in", "SourceGraphic");

merge.appendChild(mergeNode3);
merge.appendChild(mergeNode2);
merge.appendChild(mergeNode1);
merge.appendChild(mergeNode4);

var overFilter = createSVGElement("filter");
overFilter.setAttribute("id", "overFilter");
overFilter.setAttribute("filterUnits", "objectBoundingBox");
overFilter.setAttribute("x", "0");
overFilter.setAttribute("y", "0");
overFilter.setAttribute("width", "3.5");
overFilter.setAttribute("height", "4");
overFilter.appendChild(off1);
overFilter.appendChild(flood1);
overFilter.appendChild(flood12);
overFilter.appendChild(overComposite1);
overFilter.appendChild(off2);
overFilter.appendChild(flood2);
overFilter.appendChild(overComposite2);
overFilter.appendChild(off3);
overFilter.appendChild(flood3);
overFilter.appendChild(overComposite3);
overFilter.appendChild(merge);

defsElement.appendChild(overFilter);

rootSVGElement.setAttribute("height", "200");

var rect1 = createSVGElement("circle");
rect1.setAttribute("cx", "100");
rect1.setAttribute("cy", "50");
rect1.setAttribute("r", "50");
rect1.setAttribute("fill", "#408067");
rect1.setAttribute("filter", "url(#overFilter)");

rootSVGElement.appendChild(rect1);

shouldBeEqualToString("overComposite1.getAttribute('in')", "F12");

function executeTest() {
    overComposite1.setAttribute("in", "F1");
    shouldBeEqualToString("overComposite1.getAttribute('in')", "F1");

    completeTest();
}

startTest(rootSVGElement, 100, 100);

var successfullyParsed = true;
