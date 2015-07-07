description("Tests wheter SVG event bubbling works accross shadow trees.");

if (window.testRunner)
    testRunner.waitUntilDone();

var svgNS = "http://www.w3.org/2000/svg";
var xhtmlNS = "http://www.w3.org/1999/xhtml";
var expected = new Array(4);
var tests = 4;
var counter = 0;
var eventCounter = 0;

function log(message) {
    var logDiv = document.getElementById("console");
    var newDiv = document.createElementNS(xhtmlNS, "div");
    newDiv.appendChild(document.createTextNode(message));
    logDiv.appendChild(newDiv);
}

function eventHandler(evt, label) {
    var targetId = evt.target.correspondingElement ? evt.target.correspondingElement.id : evt.target.id;
    var curTargetId = evt.currentTarget.correspondingElement ? evt.currentTarget.correspondingElement.id : evt.currentTarget.id;

    var phaseString = "";
    switch (evt.eventPhase) {
    case 1: phaseString = "CAPTURING"; break;
    case 2: phaseString = "AT_TARGET"; break;
    case 3: phaseString = "BUBBLING"; break;
    }

    msg = '[EventHandler ' + label + '] type: ' + evt.type + ' phase: ' + phaseString +
          ' target: '         + evt.target        + ' (id: ' + targetId    + ')' +
          ' currentTarget: '  + evt.currentTarget + ' (id: ' + curTargetId + ')';

    shouldBeEqualToString("msg", expected[eventCounter]);
    ++eventCounter;

    if (label == counter)
        setTimeout(label == tests ? finishTest : nextTest, 0);
}

function finishTest()
{
    successfullyParsed = true;

    use.instanceRoot.correspondingElement.setAttribute("fill", "green");
    shouldBeTrue("successfullyParsed");
    debug('<br /><span class="pass">TEST COMPLETE</span>');

    if (window.testRunner)
        testRunner.notifyDone();
}

function nextTest()
{
    eventCounter = 0;
    ++counter;

    switch (counter) {
    case 1:
        rect.onclick = function(evt) { eventHandler(evt, 1); };
        expected[0] = "[EventHandler 1] type: click phase: AT_TARGET target: [object SVGElementInstance] (id: rect) currentTarget: [object SVGElementInstance] (id: rect)";
        testListeners();
        break;
    case 2:
        rectContainer.addEventListener("click", function(evt) { eventHandler(evt, 2) }, false);
        expected[1] = "[EventHandler 2] type: click phase: BUBBLING target: [object SVGElementInstance] (id: rect) currentTarget: [object SVGElementInstance] (id: rectParent)";    
        testListeners();
        break;
    case 3:
        use.setAttribute("onclick", "eventHandler(evt, 3)");
        expected[2] = "[EventHandler 3] type: click phase: BUBBLING target: [object SVGElementInstance] (id: rect) currentTarget: [object SVGUseElement] (id: use)";
        testListeners();
        break;
    case 4:
        useContainer.onclick = function(evt) { eventHandler(evt, 4) };
        expected[3] = "[EventHandler 4] type: click phase: BUBBLING target: [object SVGElementInstance] (id: rect) currentTarget: [object SVGGElement] (id: useParent)";
        testListeners();
        break;
    }
}

function testListeners()
{
    if (window.eventSender) {
        eventSender.mouseMoveTo(50, 50);
        eventSender.mouseDown();
        eventSender.mouseUp();
    }
}

// Create root element
var svg = document.createElementNS(svgNS, "svg");
svg.id = "rootSVG";
svg.setAttribute("width", 100);
svg.setAttribute("height", 100);
document.body.insertBefore(svg, document.body.firstChild);

// Create defs section
var defs = document.createElementNS(svgNS, "defs");
svg.appendChild(defs);

var rectContainer = document.createElementNS(svgNS, "g");
rectContainer.id = "rectParent";
defs.appendChild(rectContainer);

var rect = document.createElementNS(svgNS, "rect");
rect.id = "rect";
rect.style.fill = "red";
rect.width.baseVal.value = 100;
rect.height.baseVal.value = 100;
rectContainer.appendChild(rect);

// Create content section
var useContainer = document.createElementNS(svgNS, "g");
useContainer.id = "useParent";
svg.appendChild(useContainer);

var use = document.createElementNS(svgNS, "use");
use.id = "use";
use.href.baseVal = "#rectParent";
useContainer.appendChild(use);

function repaintTest() {
    if (window.testRunner)
        testRunner.waitUntilDone();
    nextTest();
}
