function log(s)
{
    document.getElementById("console").appendChild(document.createTextNode(s + "\n"));
}

function shouldBe(a, b)
{
    var evalA, evalB;
    try {
        evalA = eval(a);
        evalB = eval(b);
    } catch(e) {
        evalA = e;
    }

    var message = (evalA === evalB)
                    ? "PASS: " + a + " should be '" + evalB + "' and is."
                    : "*** FAIL: " + a + " should be '" + evalB + "' but instead is " + evalA + ". ***";
    log(message);
}

function shouldBeTrue(a) 
{ 
    shouldBe(a, "true"); 
}

function shouldBeFalse(b) 
{ 
    shouldBe(b, "false"); 
}

function canGet(keyPath)
{
    try {
        return eval("window." + keyPath) !== undefined;
    } catch(e) {
        return false;
    }
}

window.marker = { };

function canSet(keyPath, valuePath)
{
    if (valuePath === undefined)
        valuePath = "window.marker";

    try {
        eval("window." + keyPath + " = " + valuePath);
        return eval("window." + keyPath) === eval("window." + valuePath);
    } catch(e) {
        return false;
    }
}

function canCall(keyPath, argumentString)
{
    try {
        eval("window." + keyPath + "(" + (argumentString === undefined ? "" : "'" + argumentString + "'") + ")");
        return true;
    } catch(e) {
        return false;
    }
}

function toString(expression, valueForException)
{
    if (valueForException === undefined)
        valueForException = "[exception]";
        
    try {
        var evalExpression = eval(expression);
        if (evalExpression === undefined)
            throw null;
        return String(evalExpression);
    } catch(e) {
        return valueForException;
    }
}
