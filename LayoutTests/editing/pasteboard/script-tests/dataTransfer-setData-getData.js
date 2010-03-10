description("Tests correct behavior of event.dataTransfer.setData/getData for 'URL', 'text/uri-list' and 'text/plain'");

// Important that we put these at the top of the doc so that logging does not cause it to go out of view (and be undragable)
var dragMe = document.createElement("div");
dragMe.innerHTML = "<span id='dragme'>Drag me</span>";
document.body.insertBefore(dragMe, document.body.firstChild);
var dragTarget = document.createElement("div");
dragTarget.innerHTML = "Drag here"
dragTarget.style.backgroundColor = "blue";
dragTarget.style.width = "100px";
dragTarget.style.height = "100px";
document.body.insertBefore(dragTarget, dragMe);

var setDataType;
var setDataValues;
var getDataType;
var getDataValues;
var getDataResult;
var getDataResultType;
var getDataLines;

var CRLF = "\r\n";

dragMe.addEventListener("dragstart", function() {
    event.dataTransfer.setData(setDataType, setDataValues.join(CRLF));
}, false);

dragTarget.addEventListener("dragenter", function() {
    event.dataTransfer.dropEffect = "copy";
    event.preventDefault();
}, false);

dragTarget.addEventListener("dragover", function() {
    event.dataTransfer.dropEffect = "copy";
    event.preventDefault();
}, false);

dragTarget.addEventListener("drop", function() {
    doDrop();
    event.preventDefault();
}, false);

function moveMouseToCenterOfElement(element)
{
    var centerX = element.offsetLeft + element.offsetWidth / 2;
    var centerY = element.offsetTop + element.offsetHeight / 2;
    eventSender.mouseMoveTo(centerX, centerY);
}

function dragOntoDragTarget() {
    var e = document.getElementById("dragme");
    window.getSelection().setBaseAndExtent(e, 0, e, 4); 
    x = e.offsetLeft + 10;
    y = e.offsetTop + e.offsetHeight / 2;
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.leapForward(400);
    moveMouseToCenterOfElement(dragTarget);
    eventSender.mouseUp();
}

function doDrop() {
    getDataResult = event.dataTransfer.getData(getDataType);
    getDataResultType = typeof getDataResult;
    shouldBeEqualToString("getDataResultType", "string");
    getDataLines = getDataResult.split(CRLF);    
    // remove potential comments
    getDataLines = getDataLines.filter(function(line) {
      return line[0] !== '#';
    });
    shouldEvaluateTo("getDataLines.length", getDataValues.length);
    for (i = 0; i < getDataLines.length && i < getDataValues.length; ++i) {
        shouldBeEqualToString("getDataLines[" + i + "].replace(/\\/$/, '')", getDataValues[i]);
    }
}

function test(setType, setValues, getType, getValues) {
    setDataType = setType;
    setDataValues = setValues;
    getDataType = getType;
    getDataValues = getValues;
    dragOntoDragTarget();
}

function runTest()
{
    debug("--- Test set/get 'URL':");
    test("URL", ["http://test.com"], 
         "URL", ["http://test.com"]);

    debug("--- Test set/get 'text/uri-list':");
    test("text/uri-list", ["http://test.com", "http://check.com"], 
         "text/uri-list", ["http://test.com", "http://check.com"]);

    debug("--- Test set 'text/uri-list', get 'URL':");
    test("text/uri-list", ["http://test.com", "http://check.com"], 
         "URL", ["http://test.com"]);

    debug("--- Test set 'text/uri-list', get 'URL', using only '\\n':");
    test("text/uri-list", ["http://test.com\nhttp://check.com"], 
         "URL", ["http://test.com"]);

    debug("--- Test set/get 'text/uri-list' with comments:");
    test("text/uri-list", ["# comment", "http://test.com", "http://check.com"], 
         "text/uri-list", ["http://test.com", "http://check.com"]);

    debug("--- Test set/get 'text/plain':");
    test("text/plain", ["Lorem ipsum dolor sit amet."], 
         "text/plain", ["Lorem ipsum dolor sit amet."]);
}

if (window.eventSender) {
    runTest();
    // Clean up after ourselves
    dragMe.parentNode.removeChild(dragMe);
    dragTarget.parentNode.removeChild(dragTarget);
} else {
    testFailed("This test is not interactive, please run using DumpRenderTree");
}

var successfullyParsed = true;

