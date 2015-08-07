function breakpointActions(a, b)
{
    // Only preserve this message on the current test page load.
    InspectorTestProxy.addResult("inside breakpointActions a:(" + a + ") b:(" + b + ")");
}
