description("test to determine whether auto-rotate animateMotion path animations pre-muliply or post-multiply animation transform matrix");
createSVGTestCase();

rootSVGElement.setAttribute("width", 800)

var text = createSVGElement("text")
text.setAttribute("transform", "translate(300, 30)")
text.textContent = "This is some text."
text.setAttribute("onclick", "executeTest()")

var animateMotion = createSVGElement("animateMotion")
animateMotion.setAttribute("id", "animation")
animateMotion.setAttribute("dur", "1s")
animateMotion.setAttribute("repeatCount", "1")
animateMotion.setAttribute("rotate", "auto")
animateMotion.setAttribute("path", "M 100,250 C 100,50 400,50 400,250")
animateMotion.setAttribute("begin", "click")
text.appendChild(animateMotion)
rootSVGElement.appendChild(text)

function passIfCloseEnough(name, value, error) {
    passed = isCloseEnough(eval(name), value, error);
    if (passed) {
        testPassed(name + " is almost " + value + " (within " + error + ")");
    } else {
        testFailed(name + " is " + eval(name) + " but should be within " + error + " of " + value);  
    }
}

function startSample() {
    passIfCloseEnough("rootSVGElement.getBBox().x", 196, 20);
    passIfCloseEnough("rootSVGElement.getBBox().y", -186, 20);
}

function endSample() {
    passIfCloseEnough("rootSVGElement.getBBox().x", 370, 20);
    passIfCloseEnough("rootSVGElement.getBBox().y", 547, 20);
}

function executeTest() {
    const expectedValues = [
        ["animation", 0.1, "text", startSample],
        ["animation", 1.0, "text", endSample]
    ];
    
    runAnimationTest(expectedValues);
}

window.setTimeout("triggerUpdate(310, 30)", 0);
var successfullyParsed = true;

