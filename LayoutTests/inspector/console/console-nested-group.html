<html>
<head>
<script src="../../http/tests/inspector/inspector-test.js"></script>
<script src="../../http/tests/inspector/console-test.js"></script>
<script>

function onload()
{
    console.group("outer group");
    console.group("inner group");
    console.log("Message inside inner group");
    console.groupEnd();
    console.groupEnd();
    console.log("Message that must not be in any group");


    var groupCount = 3;
    for (var i = 0; i < groupCount; i++) {
        console.group("One of several groups which shouldn't be coalesced.");
    }
    console.log("Message inside third group");
    for (var i = 0; i < groupCount; i++) {
        console.groupEnd();
    }

    runTest();
}

function test()
{
    InspectorTest.dumpConsoleMessagesWithClasses();
    InspectorTest.completeTest();
}
</script>
</head>

<body onload="onload()">
<p>
Tests that console.group/groupEnd messages won't be coalesced. <a href="https://bugs.webkit.org/show_bug.cgi?id=56114">Bug 56114.</a>
<a href="https://bugs.webkit.org/show_bug.cgi?id=63521">Bug 63521.</a>

</p>

</body>
</html>
