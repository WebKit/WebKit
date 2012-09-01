description("test to determine whether auto-rotate animateMotion path animations pre-muliply or post-multiply animation transform matrix");
createSVGTestCase();

rootSVGElement.setAttribute("width", 800)

var g = createSVGElement("g")
g.setAttribute("id", "g");
g.setAttribute("transform", "translate(300, 30)")

var rect = createSVGElement("rect")
rect.setAttribute("id", "rect")
rect.setAttribute("width", "40")
rect.setAttribute("height", "40")
rect.setAttribute("fill", "green")
rect.setAttribute("onclick", "executeTest()")
g.appendChild(rect)

var animateMotion = createSVGElement("animateMotion")
animateMotion.setAttribute("id", "animation")
animateMotion.setAttribute("dur", "40s")
animateMotion.setAttribute("repeatCount", "1")
animateMotion.setAttribute("rotate", "auto")
animateMotion.setAttribute("path", "M 100,250 C 100,50 400,50 400,250")
animateMotion.setAttribute("begin", "click")
g.appendChild(animateMotion)
rootSVGElement.appendChild(g)

function startSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "131", 1);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "-90", 1);
}

function endSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "331", 1);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "550", 1);
}

function executeTest() {
    const expectedValues = [
        ["animation", 0.001, startSample],
        ["animation", 39.999, endSample]
    ];
    
    runAnimationTest(expectedValues);
}

window.clickX = 310;
window.clickY = 40;
var successfullyParsed = true;
