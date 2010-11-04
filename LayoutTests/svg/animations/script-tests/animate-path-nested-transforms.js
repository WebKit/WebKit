description("test to determine whether auto-rotate animateMotion path animations pre-muliply or post-multiply animation transform matrix");
createSVGTestCase();

rootSVGElement.setAttribute("width", 800)

var g = createSVGElement("g")
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
animateMotion.setAttribute("dur", "1s")
animateMotion.setAttribute("repeatCount", "1")
animateMotion.setAttribute("rotate", "auto")
animateMotion.setAttribute("path", "M 100,250 C 100,50 400,50 400,250")
animateMotion.setAttribute("begin", "click")
g.appendChild(animateMotion)
rootSVGElement.appendChild(g)

function passIfCloseEnough(name, value, error) {
    passed = isCloseEnough(eval(name), value, error);
    if (passed) {
        testPassed(name + " is almost " + value + " (within " + error + ")");
    } else {
        testFailed(name + " is " + eval(name) + " but should be within " + error + " of " + value);  
    }
}

function startSample() {
    passIfCloseEnough("rootSVGElement.getBBox().x", 400, 20);
    passIfCloseEnough("rootSVGElement.getBBox().y", 240, 20);
}

function endSample() {
    passIfCloseEnough("rootSVGElement.getBBox().x", 660, 20);
    passIfCloseEnough("rootSVGElement.getBBox().y", 270, 20);
}

function executeTest() {
    const expectedValues = [
        ["animation", 0.01, "rect", startSample],
        ["animation", 0.99, "rect", endSample]
    ];
    
    runAnimationTest(expectedValues);
}

window.setTimeout("triggerUpdate(310, 40)", 0);
var successfullyParsed = true;

