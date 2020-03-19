function isGtk()
{
    return navigator.userAgent.includes("WebKitTestRunnerGTK");
}

function isEfl()
{
    return navigator.userAgent.includes("WebKitTestRunnerEFL");
}

function videoCanvasPixelComparisonTolerance()
{
    if (isGtk() || isEfl())
        return 6;

    return 2;
}

function checkPixelColorWithTolerance(pixel, r, g, b, a)
{
    const tolerance = videoCanvasPixelComparisonTolerance();
    return Math.abs(pixel[0] - r) <= tolerance
        && Math.abs(pixel[1] - g) <= tolerance
        && Math.abs(pixel[2] - b) <= tolerance
        && Math.abs(pixel[3] - a) <= tolerance;
}

function isPixelBlack(pixel)
{
    return checkPixelColorWithTolerance(pixel, 0, 0, 0, 255);
}

function isPixelTransparent(pixel)
{
    return checkPixelColorWithTolerance(pixel, 0, 0, 0, 0);
}

function isPixelWhite(pixel)
{
    return checkPixelColorWithTolerance(pixel, 255, 255, 255, 255);
}

function isPixelGray(pixel)
{
    return checkPixelColorWithTolerance(pixel, 128, 128, 128, 255);
}
