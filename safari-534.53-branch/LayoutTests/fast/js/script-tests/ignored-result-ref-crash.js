description(
"This tests that bytecode code generation doesn't crash when it encounters odd cases of an ignored result."
);

function emptyStatementDoWhileTest()
{
    do
        ;
    while (false) { }
    return true;
}

shouldBeTrue("emptyStatementDoWhileTest()");

function debuggerDoWhileTest()
{
    do
        debugger;
    while (false) { }
    return true;
}

shouldBeTrue("debuggerDoWhileTest()");

function continueDoWhileTest()
{
    var i = 0;
    do
        i++;
    while (i < 10) {
        do
            continue;
        while (false) { }
    }
    return true;
}

shouldBeTrue("continueDoWhileTest()");

function breakDoWhileTest()
{
    var i = 0;
    do
        i++;
    while (i < 10) {
        do
            continue;
        while (false) { }
    }
    return true;
}

shouldBeTrue("breakDoWhileTest()");

function tryDoWhileTest()
{
    do
        try { } catch (o) { }
    while (false) { }
    return true;
}

shouldBeTrue("tryDoWhileTest()");

var successfullyParsed = true;
