description("test to determine whether auto-rotate animateMotion path animations pre-muliply or post-multiply animation transform matrix");
createSVGTestCase();

rootSVGElement.setAttribute("width", 800)

var text = createSVGElement("text")
text.setAttribute("transform", "translate(300, 30)")
text.textContent = "This is some text."
text.setAttribute("onclick", "executeTest()")

var animateMotion = createSVGElement("animateMotion")
animateMotion.setAttribute("id", "animation")
animateMotion.setAttribute("dur", "4s")
animateMotion.setAttribute("repeatCount", "1")
animateMotion.setAttribute("rotate", "auto")
animateMotion.setAttribute("path", "M 100,250 C 100,50 400,50 400,250")
animateMotion.setAttribute("begin", "click")
text.appendChild(animateMotion)
rootSVGElement.appendChild(text)

function startSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "118.65", 0.01);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "-161.03", 0.01);
}

function endSample() {
    shouldBeCloseEnough("rootSVGElement.getBBox().x", "366.89", 1);
    shouldBeCloseEnough("rootSVGElement.getBBox().y", "549.93", 1);
}

function executeTest() {
    const expectedValues = [
        ["animation", 0.01, startSample],
        ["animation", 3.999, endSample]
    ];
    
    runAnimationTest(expectedValues);
}

window.clickX = 310;
window.clickY = 30;
var successfullyParsed = true;
