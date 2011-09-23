function isDebugEnabled()
{
    // Add #debug at the end of the url to visually debug the test case.
    return window.location.hash == "#debug";
}

function rectToArray(rect) {
    return [rect.top, rect.left, rect.bottom, rect.right, rect.width, rect.height];
}

function areEqualNumbers(actual, expected, tolerance) {
    var diff = Math.abs(actual - expected);
    return diff <= tolerance;
}

function areEqualRects(r1, r2, tolerance) {
    if (r1.length != r2.length)
        return false;
    
    for (var i = 0; i < r1.length; ++i)
        if (!areEqualNumbers(r1[i], r2[i], tolerance))
            return false;
    
    return true;
}

function assertEqualRects(results, name, actualRect, expectedRect, tolerance) {
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

function assertRectContains(results, name, containerRect, insideRect, tolerance) {
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

function addPageLevelDebugBox(rect, color) {
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