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
