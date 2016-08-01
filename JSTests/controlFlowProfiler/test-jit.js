load("./driver/driver.js");

function tierUpToBaseline(func, arg) 
{
    for (var i = 0; i < 50; i++)
        func(arg);
}

function tierUpToDFG(func, arg) 
{
    for (var i = 0; i < 50; i++)
        func(arg);
}

function baselineTest(arg) {
    if (arg > 20) {
        return 20;
    } else {
        return 30;
    }
}

function dfgTest(arg) {
    if (arg > 20) {
        return 20;
    } else {
        return 30;
    }
}

noInline(baselineTest);
noInline(dfgTest);

tierUpToBaseline(baselineTest, 10);
tierUpToDFG(dfgTest, 10);

assert(!hasBasicBlockExecuted(baselineTest, "return 20"), "should not have executed yet.");
assert(hasBasicBlockExecuted(baselineTest, "return 30"), "should have executed.");
baselineTest(25);
assert(hasBasicBlockExecuted(baselineTest, "return 20"), "should have executed.");

assert(!hasBasicBlockExecuted(dfgTest, "return 20"), "should not have executed yet.");
assert(hasBasicBlockExecuted(dfgTest, "return 30"), "should have executed.");
dfgTest(25);
assert(hasBasicBlockExecuted(dfgTest, "return 20"), "should have executed.");

