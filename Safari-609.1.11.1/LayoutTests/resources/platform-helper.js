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
