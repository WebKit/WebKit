var initialize_SetOuterHTMLTest = function() {

InspectorTest.events = [];
InspectorTest.containerId;

InspectorTest.setUpTestSuite = function(next)
{
    InspectorTest.expandElementsTree(step1);

    function step1()
    {
        InspectorTest.selectNodeWithId("container", step2);
    }

    function step2(node)
    {
        InspectorTest.containerId = node.id;
        DOMAgent.getOuterHTML(InspectorTest.containerId, step3);
    }

    function step3(error, text)
    {
        InspectorTest.containerText = text;

        for (var key in WebInspector.DOMAgent.Events) {
            var eventName = WebInspector.DOMAgent.Events[key];
            WebInspector.domAgent.addEventListener(eventName, InspectorTest.recordEvent.bind(InspectorTest, eventName));
        }

        next();
    }
}

InspectorTest.recordEvent = function(eventName, event)
{
    var node = event.data.node || event.data;
    var parent = event.data.parent;
    for (var currentNode = parent || node; currentNode; currentNode = currentNode.parentNode) {
        if (currentNode.getAttribute("id") === "output")
            return;
    }
    InspectorTest.events.push("Event " + eventName + ": " + node.nodeName());
}

InspectorTest.patchOuterHTML = function(pattern, replacement, next)
{
   InspectorTest.addResult("Replacing '" + pattern + "' with '" + replacement + "'\n");
   InspectorTest.setOuterHTML(InspectorTest.containerText.replace(pattern, replacement), next);
}

InspectorTest.setOuterHTML = function(newText, next)
{
    InspectorTest.innerSetOuterHTML(newText, false, bringBack);

    function bringBack()
    {
        InspectorTest.addResult("\nBringing things back\n");
        InspectorTest.innerSetOuterHTML(InspectorTest.containerText, true, next);
    }
}

InspectorTest.innerSetOuterHTML = function(newText, last, next)
{
    DOMAgent.setOuterHTML(InspectorTest.containerId, newText, dumpOuterHTML);

    function dumpOuterHTML()
    {
        RuntimeAgent.evaluate("document.getElementById(\"identity\").wrapperIdentity", dumpIdentity);
        function dumpIdentity(error, result)
        {
            InspectorTest.addResult("Wrapper identity: " + result.value);
            InspectorTest.events.sort();
            for (var i = 0; i < InspectorTest.events.length; ++i)
                InspectorTest.addResult(InspectorTest.events[i]);
            InspectorTest.events = [];
        }

        DOMAgent.getOuterHTML(InspectorTest.containerId, callback);

        function callback(error, text)
        {
            InspectorTest.addResult("==========8<==========");
            InspectorTest.addResult(text);
            InspectorTest.addResult("==========>8==========");
            if (last)
                InspectorTest.addResult("\n\n\n");
            next();
        }
    }
}

};
