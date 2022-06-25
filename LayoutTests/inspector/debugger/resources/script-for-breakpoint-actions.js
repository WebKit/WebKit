function breakpointActions(a, b)
{
    // Only preserve this message on the current test page load.
    TestPage.addResult("inside breakpointActions a:(" + a + ") b:(" + b + ")");
}
