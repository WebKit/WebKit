function breakpointBasic()
{
    log("inside breakpointBasic");
}

function breakpointWithCondition(a, b)
{
    log("inside breakpointWithCondition a:(" + a + ") b:(" + b + ") a+b:(" + (a+b) + ")");
}

function breakpointAutomaticallyContinue()
{
    log("inside breakpointAutomaticallyContinue");
}

function breakpointActions(a, b)
{
    log("inside breakpointActions a:(" + a + ") b:(" + b + ")");
}

function notInlineable(x)
{
    var func = new Function("return x + 100;");
    return x + 3;
}

function inlineable(x)
{
    return x + 5;
}

function notInliningFoo(x)
{
    return notInlineable(x);
}

function inliningFoo(x)
{
    return inlineable(x);
}

function dfgWithoutInline()
{
    var i;
    var result = 0;
    for (i = 0; i < 1000; i++)
        result += notInliningFoo(i);
    log("dfgWithoutInline result: " + result);    
}

function dfgWithInline()
{
    var i;
    var result = 0;
    for (i = 0; i < 1000; i++)
        result += inliningFoo(i);
    log("dfgWithInline result: " + result);    
}

function notInlineable2(x, callFunctionWithBreakpoint)
{
    var func = new Function("return x + 100;");
    if (callFunctionWithBreakpoint)
        breakpointBasic();
    return x + 3;
}

function dfgWithoutInline2()
{
    var i;
    var result = 0;
    for (i = 0; i < 1000; i++)
        result += notInlineable2(i, false);
    log("result: " + result);
}

function callNotInlineable2()
{
    var result = notInlineable2(10000, true);
    if (result == 20003)
        log("PASS: result is " + result);
    else
        log("FAIL: result is " + result + ", expecting 20003");
}

function debuggerStatement(x)
{
    log("In function with debugger statement");
    debugger;
    log("After debugger statement");
    return x + 3;
}

