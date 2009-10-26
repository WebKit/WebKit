function TestMutation(remover, removee, result)
{
    var report = [];

    var node = document.createElement("button");
    var eventType = "click";
    document.body.appendChild(node);
    
    var listeners = [
      function() { mutateList(0); },
      function() { mutateList(1); },
      function() { mutateList(2); }
    ];

    listeners.forEach(function(listener) { node.addEventListener(eventType, listener, false); });

    node.click();
    document.body.removeChild(node);

    var log = "listener " + remover + " removing listener " + removee;

    if (report.join(" ") == result)
        testPassed(log);
    else
        testFailed(log);

    function mutateList(me)
    {
        if (remover == me)
            node.removeEventListener(eventType, listeners[removee], false);
        report.push(me);
    }
}
 
description("Tests that event list mutation preserves the order of event firing.");

debug("self-removal:");
TestMutation(0, 0, "0 1 2");
TestMutation(1, 1, "0 1 2");
TestMutation(2, 2, "0 1 2");

debug("successor removal:");
TestMutation(0, 1, "0 2");
TestMutation(0, 2, "0 1");

debug("predecessor removal:");
TestMutation(2, 0, "0 1 2");
TestMutation(2, 1, "0 1 2");

var successfullyParsed = true;