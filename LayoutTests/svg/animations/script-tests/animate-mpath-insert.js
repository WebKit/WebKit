description("test to determine whether inserting mpath dynamically works");
createSVGTestCase();

rootSVGElement.setAttribute("width", 800)

var defs = createSVGElement("defs")
var path = createSVGElement("path")
path.setAttribute("id", "path")
path.setAttribute("d", "M 100,250 C 100,50 400,50 400,250")
defs.appendChild(path)
rootSVGElement.appendChild(defs)

var g = createSVGElement("g")

var rect = createSVGElement("rect")
rect.setAttribute("id", "rect")
rect.setAttribute("width", "40")
rect.setAttribute("height", "40")
rect.setAttribute("fill", "green")
rect.setAttribute("onclick", "executeTest()")
g.appendChild(rect)

var animateMotion = createSVGElement("animateMotion")
animateMotion.setAttribute("id", "animation")
animateMotion.setAttribute("dur", "4s")
animateMotion.setAttribute("repeatCount", "1")
animateMotion.setAttribute("begin", "click")

var mpath = createSVGElement("mpath");
mpath.setAttributeNS(xlinkNS, "xlink:href", "#path")

animateMotion.appendChild(mpath)
g.appendChild(animateMotion)
rootSVGElement.appendChild(g)

function startSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "100", 5);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "250", 5);
}

function endSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "400", 5);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "250", 5);
}

function executeTest() {
    const expectedValues = [
        ["animation", 0.02, "rect", startSample],
        ["animation", 3.99, "rect", endSample]
    ];
    
    runAnimationTest(expectedValues);
}

window.setTimeout("triggerUpdate(10, 40)", 0);
var successfullyParsed = true;

