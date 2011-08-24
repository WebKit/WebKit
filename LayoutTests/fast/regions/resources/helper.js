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

    results.push("FAIL(" + name + " bounding rect was: [" + actualRect.join(", ") + "], expected: [" + expectedRect.join(", ") + "]");
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