function print(text)
{
    var output = document.getElementById("output");
    var div = document.createElement("div");
    div.textContent = text;
    output.appendChild(div);
}

function dumpConsoleMessageArgumentCounts()
{
    var consoleMessageArgumentCounts = window.internals.consoleMessageArgumentCounts();
    if (consoleMessageArgumentCounts.length === 3)
        print("PASSED: found argument counts for 3 messages");
    else
        print("FAILED: unexpected number of messages: " + consoleMessageArgumentCounts.length);

    for (var i = 0; i < consoleMessageArgumentCounts.length; i++) {
        var count = consoleMessageArgumentCounts[i];
        if (count == 0)
            print("PASSED: message #" + i + " has no arguments.");
        else
            print("FAILED: message #" + i + " has " + count + " arguments.");
    }
}
