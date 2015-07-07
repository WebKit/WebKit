// [Name] SVGFETileElement-dom-in-attr.js
// [Expected rendering result] A rectangle tiled with green rectangles - and a series of PASS messages

description("Tests dynamic updates of the 'in' attribute of the SVGFETileElement object")
createSVGTestCase();

var defsElement = createSVGElement("defs");
rootSVGElement.appendChild(defsElement);

var flood = createSVGElement("feFlood");
flood.setAttribute("x", "115");
flood.setAttribute("y", "40");
flood.setAttribute("width", "54");
flood.setAttribute("height", "19");
flood.setAttribute("flood-color", "green");

var offset = createSVGElement("feOffset");
offset.setAttribute("x", "115");
offset.setAttribute("y", "40");
offset.setAttribute("width", "50");
offset.setAttribute("height", "25");
offset.setAttribute("dx", "6");
offset.setAttribute("dy", "6");
offset.setAttribute("result", "offset");

var tile = createSVGElement("feTile");
tile.setAttribute("in", "SourceGraphic");

var tileFilter = createSVGElement("filter");
tileFilter.setAttribute("id", "myFilter");
tileFilter.setAttribute("filterUnits", "userSpaceOnUse");
tileFilter.setAttribute("primitiveUnits", "userSpaceOnUse");
tileFilter.setAttribute("x", "15");
tileFilter.setAttribute("y", "40");
tileFilter.setAttribute("width", "400");
tileFilter.setAttribute("height", "250");
tileFilter.appendChild(flood);
tileFilter.appendChild(offset);
tileFilter.appendChild(tile);

defsElement.appendChild(tileFilter);

var rect1 = createSVGElement("rect");
rect1.setAttribute("x", "15");
rect1.setAttribute("y", "40");
rect1.setAttribute("width", "350");
rect1.setAttribute("height", "250");
rect1.setAttribute("fill", "none");
rect1.setAttribute("stroke", "blue");
rect1.setAttribute("stroke-width", "2");

var gElement = createSVGElement("g");
gElement.setAttribute("filter", "url(#myFilter)");
gElement.appendChild(rect1);

rootSVGElement.setAttribute("width", "370");
rootSVGElement.appendChild(gElement);

shouldBeEqualToString("tile.getAttribute('in')", "SourceGraphic");

function repaintTest() {
    tile.setAttribute("in", "offset");
    shouldBeEqualToString("tile.getAttribute('in')", "offset");

    completeTest();
}


var successfullyParsed = true;
