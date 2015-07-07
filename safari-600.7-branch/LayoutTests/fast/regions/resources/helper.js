function isDebugEnabled()
{
    // Add #debug at the end of the url to visually debug the test case.
    return window.location.hash == "#debug";
}

function getFlowByName(name)
{
    var namedFlows = document.webkitGetNamedFlows();
    return namedFlows[name] ? namedFlows[name] : null;
}

function rectToArray(rect)
{
    return [rect.top, rect.left, rect.bottom, rect.right, rect.width, rect.height];
}

function areEqualNumbers(actual, expected, tolerance)
{
    var diff = Math.abs(actual - expected);
    return diff <= tolerance;
}

function areEqualRects(r1, r2, tolerance)
{
    if (r1.length != r2.length)
        return false;
    
    for (var i = 0; i < r1.length; ++i)
        if (!areEqualNumbers(r1[i], r2[i], tolerance))
            return false;
    
    return true;
}

function assertTopLeftMatch (r1, r2, tolerance)
{
    if (sameTopLeft(r1, r2, tolerance))
        return "PASS";
    return "FAIL. Expected top left points to match, but got ("+ r1.top + "," + r1.left + ") and ("+ r2.top + "," + r2.left + ")";
}

function sameTopLeft(r1, r2, tolerance)
{
    if (tolerance === undefined)
        tolerance = 0;
    if ( areEqualNumbers(r1.top, r2.top, tolerance) && areEqualNumbers(r1.left, r2.left, tolerance) )
        return true;
    return false;
}

function assertEqualRects(results, name, actualRect, expectedRect, tolerance)
{
    if (areEqualRects(actualRect, expectedRect, tolerance))
        return;

    results.push("FAIL(" + name + ") bounding rect was: [" + actualRect.join(", ") + "], expected: [" + expectedRect.join(", ") + "]");
}

function testBoundingRects(expectedBoundingRects, tolerance)
{
    if (tolerance === undefined)
        tolerance = 0;

    var results = [];

    for (var name in expectedBoundingRects) {
        if (!expectedBoundingRects.hasOwnProperty(name))
            continue;
        var rect = document.getElementById(name).getBoundingClientRect();
        assertEqualRects(results, name, rectToArray(rect), expectedBoundingRects[name], tolerance);
    }

    document.write("<p>" + (results.length ? results.join("<br />") : "PASS") + "</p>");
    
    return !results.length;
}

function testClientRects(id, expectedClientRects, tolerance)
{
    if (tolerance === undefined)
        tolerance = 0;

    var results = [];
    var actualRects = document.getElementById(id).getClientRects();
    if (actualRects.length != expectedClientRects.length)
        results.push("FAIL: The element #" + id + " has a wrong count of client rects: " + actualRects.length + "(actual) vs. " + expectedClientRects.length + "(expected)");
    else {
        for (var i = 0; i < expectedClientRects.length; ++i) {
            var expectedRect = expectedClientRects[i];
            var actualRect = actualRects[i];
            assertEqualRects(results, id, rectToArray(actualRect), expectedRect, tolerance);
        }
    }

    document.write("<p>" + (results.length ? results.join("<br />") : "PASS") + "</p>");
    
    return !results.length;
}

function assertRectContains(results, name, containerRect, insideRect, tolerance)
{
    // make the container rect bigger with tolerance
    var left = containerRect.left - tolerance;
    var right = containerRect.right + tolerance;
    var top = containerRect.top - tolerance;
    var bottom = containerRect.bottom + tolerance;
    var pass = left <= insideRect.left && insideRect.right <= right
               && top <= insideRect.top && insideRect.bottom <= bottom;
    if (!pass)
        results.push("FAIL(" + name + ") outside bounding rect was: [" + rectToArray(containerRect).join(", ") + "], and inside was: [" + rectToArray(insideRect).join(", ") + "]");
    return pass;
}

function addPageLevelDebugBox(rect, color)
{
    var el = document.createElement("div");
    el.style.position = "absolute";
    el.style.left = rect.left + "px";
    el.style.top = rect.top + "px";
    el.style.width = rect.width + "px";
    el.style.height = rect.height + "px";
    el.style.backgroundColor = color;
    document.body.appendChild(el);
}

function testContentToRegionsMapping(tolerance)
{
    if (tolerance === undefined)
        tolerance = 0;
        
    var debug = isDebugEnabled();
    var drawnRegions = {};
    
    var elements = document.getElementsByClassName("check");
    var results = [];
    for (var i = 0; i < elements.length; ++i) {
        var el = elements[i];
        var regionId = el.className.split(" ")[1];
        var region = document.getElementById(regionId);
        var regionRect = region.getBoundingClientRect();
        var contentRect = el.getClientRects();
        if (debug && !drawnRegions[regionId]) {
            addPageLevelDebugBox(regionRect, "rgba(255,0,0,0.3)");
            drawnRegions[regionId] = true;
        }
        for (var j = 0; j < contentRect.length; ++j) {
            var passed = assertRectContains(results, el.className, regionRect, contentRect[j], tolerance);
            if (debug)
                addPageLevelDebugBox(contentRect[j], passed ? "rgba(0,255,0,0.3)" : "rgba(255,0,255,0.5)");
        }
    }

    document.write("<p>" + (results.length ? results.join("<br />") : "PASS") + "</p>");

    return !results.length && !debug;
}

function logMessage(message, success)
{
    var pElem = document.createElement("p");

    var spanElement = document.createElement("span");
    spanElement.className = success ? "pass" : "fail";
    var spanTextNode = success ? document.createTextNode("PASS") : document.createTextNode("FAIL");
    spanElement.appendChild(spanTextNode);
    pElem.appendChild(spanElement);

    var textNode = document.createTextNode(message);
    pElem.appendChild(textNode);
	document.getElementById("console").appendChild(pElem);
}

function logPassMessage(message)
{
    logMessage(message, true);
}

function logFailMessage(message)
{
    var logMsg = "" + message;
    if (logMsg.length > 0)
        logMsg = " : " + logMsg;
    logMessage(logMsg, false);
}

function assert(expression, failMessage)
{
    expression ? logPassMessage("") : logFailMessage(failMessage);
}

// used by getRegionFlowRanges tests
function getName(node) {
  if (!node) return "undefined";
  if (node.nodeType == 3) // Text node
    return "#text";
  // all the others should have an id
  return node.id;
}

function getRangeAt(arrRange, index) {
  if (index < arrRange.length)
    return [getName(arrRange[index].startContainer), arrRange[index].startOffset, getName(arrRange[index].endContainer), arrRange[index].endOffset];
  return null;
}

function compareArrays(current, expected) {
    try {
        if (current == null) {
            testFailed("Null object. Expected [" + expected.toString() + "] was null");
            return;
        }
        if (current.length !== expected.length) {
            testFailed("Array length differs. Expected [" + expected.toString() + "] was [" + current.toString() + "]");
            return;
        }
        for (var i = 0; i < current.length; i++)
            if (current[i] !== expected[i]) {
                testFailed("Expected ["  + expected.toString() + "]. Was [" + current.toString() + "]");
                return;
            }
    } catch (ex) {
        testFailed(current + " threw exception " + ex);
    }
    testPassed("Array ["  + expected.toString() + "] is equal to [" + current.toString() + "]");
}

function selectContentByRange(fromX, fromY, toX, toY) {
    if (!window.testRunner)
        return;

    eventSender.mouseMoveTo(fromX, fromY);
    eventSender.mouseDown();

    eventSender.mouseMoveTo(toX, toY);
    eventSender.mouseUp();
}

function selectContentFromIdToPos(fromId, toX, toY) {
  var fromRect = document.getElementById(fromId).getBoundingClientRect();
  var fromRectVerticalCenter = fromRect.top + fromRect.height / 2;

  selectContentByRange(fromRect.left, fromRectVerticalCenter, toX, toY);
}

function selectContentFromIdToPosVert(fromId, toX, toY) {
  var fromRect = document.getElementById(fromId).getBoundingClientRect();
  var fromRectHorizontalCenter = fromRect.left + fromRect.width / 2;

  selectContentByRange(fromRectHorizontalCenter, fromRect.top, toX, toY);
}

function selectContentByIds(fromId, toId) {
    var fromRect = document.getElementById(fromId).getBoundingClientRect();
    var toRect = document.getElementById(toId).getBoundingClientRect();

    var fromRectVerticalCenter = fromRect.top + fromRect.height / 2;
    var toRectVerticalCenter = toRect.top + toRect.height / 2;

    selectContentByRange(fromRect.left, fromRectVerticalCenter, toRect.right, toRectVerticalCenter);
}

function selectContentByIdsVertical(fromId, toId) {
    var fromRect = document.getElementById(fromId).getBoundingClientRect();
    var toRect = document.getElementById(toId).getBoundingClientRect();

    var fromRectHorizontalCenter = fromRect.left + fromRect.width / 2;
    var toRectHorizontalCenter = toRect.left + toRect.width / 2;

    selectContentByRange(fromRectHorizontalCenter, fromRect.top, toRectHorizontalCenter, toRect.bottom);
}

function selectBaseAndExtent(fromId, fromOffset, toId, toOffset) {
    var from = document.getElementById(fromId);
    var to = document.getElementById(toId);

    var selection = window.getSelection();
    selection.setBaseAndExtent(from, fromOffset, to, toOffset);
}

function mouseClick(positionX, positionY) {
    if (!window.testRunner)
        return;

    eventSender.mouseMoveTo(positionX, positionY);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function onMouseUpLogSelectionAndFocus(contentId, nodeId, offsetId) {
    document.onmouseup = function() {
        var sel = window.getSelection();

        var node = sel['focusNode'];
        // nodeType == 3 for textNode
        var id = (node.nodeType != 3) ? " (id: " + node.id + ")" : " (parent id: " + node.parentNode.id + ")";

        document.getElementById(nodeId).innerHTML = id + " " + node + " " + node.nodeValue;
        document.getElementById(offsetId).innerHTML = sel['focusOffset'];
        document.getElementById(contentId).innerHTML = sel.getRangeAt(0);
    }
}

function onMouseUpLogSelection(elementId) {
    document.onmouseup = function() {
        var selectedContent = document.getElementById(elementId);
        selectedContent.innerHTML = window.getSelection().getRangeAt(0);
    }
}

function clearSelection() {
    mouseClick(0, 0);
}

function checkSelectionResult(id, expectedSelection, expectedFocusNodeId, expectedFocusOffset) {
    var sel = window.getSelection();

    // Check actual and expected selection
    var actualSelectedContent = sel.getRangeAt(0).toString().replace(/(\r\n|\n|\r)/gm,"").replace(/\s+/gm, "");
    var expectedSelectedContent = expectedSelection.replace(/(\r\n|\n|\r)/gm,"").replace(/\s+/gm, "");
    var selectionComparison = (actualSelectedContent == expectedSelectedContent);

    // Check actual and expected focus node
    var focusNodeComparison = true;
    var actualFocusNodeId;
    if (expectedFocusNodeId != undefined) {
        var node = sel['focusNode'];
        // nodeType == 3 for textNode
        actualFocusNodeId = (node.nodeType != 3) ? node.id : node.parentNode.id;
        focusNodeComparison = (actualFocusNodeId == expectedFocusNodeId);
    }

    // Check actual and expected focus offset
    var focusOffsetComparison = true;
    var actualFocusOffset;
    if (expectedFocusOffset != undefined) {
        actualFocusOffset =  sel['focusOffset'];
        focusOffsetComparison = (actualFocusOffset == expectedFocusOffset);
    }

    var result = selectionComparison && focusNodeComparison && focusOffsetComparison;
    document.getElementById(id).innerHTML = result ? "PASS" : "FAIL";

    if (!result) {
        var errorMessage = "";
        if (!selectionComparison)
            errorMessage += "\nExpected selection: " + expectedSelection + "\nActual selection: " + sel.getRangeAt(0).toString();
        if (!focusNodeComparison)
            errorMessage += "\nExpected focus node: " + expectedFocusNodeId + "\nActual focus node: " + actualFocusNodeId;
        if (!focusOffsetComparison)
            errorMessage += "\nExpected focus offset: " + expectedFocusOffset + "\nActual focus offset: " + actualFocusOffset;
        console.log(errorMessage);
    }
}
