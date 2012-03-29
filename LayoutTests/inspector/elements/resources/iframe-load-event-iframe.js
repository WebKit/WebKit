function loadSecondIFrame()
{
    document.getElementById("myframe").onload = null;
    document.getElementById("myframe").src = "resources/iframe-load-event-iframe-2.html";
}

function test()
{
    InspectorTest.expandElementsTree(step1);

    function step1()
    {
        WebInspector.domAgent.addEventListener(WebInspector.DOMAgent.Events.NodeInserted, nodeInserted);
        InspectorTest.evaluateInPage("loadSecondIFrame()");

        function nodeInserted(event)
        {
            var node = event.data;
            if (node.getAttribute("id") === "myframe") {
                InspectorTest.expandElementsTree(step2);
                WebInspector.domAgent.removeEventListener(WebInspector.DOMAgent.Events.NodeInserted, nodeInserted);
            }
        }
    }

    function step2()
    {
        InspectorTest.addResult("\n\nAfter frame navigate");
        InspectorTest.dumpElementsTree();
        InspectorTest.completeTest();
    }
}
