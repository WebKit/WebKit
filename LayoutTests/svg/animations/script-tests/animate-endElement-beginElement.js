description("Tests animation beginElement command's restarting capability after endElement.");
createSVGTestCase();

// Setup test document

var rect = createSVGElement("rect");
rect.setAttribute("id", "rect");
rect.setAttribute("width", "50px");
rect.setAttribute("height", "50px");
rect.setAttribute("fill", "green");

var animateX = createSVGElement("animate");
animateX.setAttribute("id", "animateX");
animateX.setAttribute("attributeName", "x");
animateX.setAttribute("from", "0");
animateX.setAttribute("to", "100");
animateX.setAttribute("dur", "2s");
animateX.setAttribute("begin", "indefinite");
animateX.setAttribute("fill", "freeze");
rect.appendChild(animateX);
rootSVGElement.appendChild(rect);

// Setup animation test

function sample1() {
    // Check half-time conditions
    shouldBe("rect.x.animVal.value", "50");
}

function startRestart() {
    animateX.beginElement();
    setTimeout(end,0);
}

function end() {
    animateX.endElement();
    setTimeout(begin,0);
}

function begin() {
    animateX.beginElement();      
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animateX", 1.0, "rect", sample1]
    ];
    runAnimationTest(expectedValues);
}

function executeTest() {
    //BeginElement-endElement-beginElement musn't execute in zero time, because in the current
    //implemetation of the svg animation will loose the commands order!
    startRestart();        
}

// Begin test async
rect.setAttribute("onclick", "executeTest()");
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;

