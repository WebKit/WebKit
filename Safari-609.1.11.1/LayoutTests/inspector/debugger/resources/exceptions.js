function triggerUncaughtTypeException()
{
    // Exception: TypeError: undefined is not an object (evaluating '[].x.x')
    [].x.x;
}

function triggerUncaughtReferenceException()
{
    // Exception: ReferenceError: Can't find variable: variableThatDoesNotExist
    variableThatDoesNotExist += 1;
}

function triggerUncaughtSyntaxException()
{
    // Exception: SyntaxError: Unexpected token ')'
    eval("if()");
}

function triggerUncaughtDOMException()
{
    // IndexSizeError (DOM Exception 1): The index is not in the allowed range.
    document.createTextNode("").splitText(100);
}

function throwString()
{
    throw "thrown string";
}

function throwNumber()
{
    throw 123.456;
}

function throwNull()
{
    throw null;
}

function throwObject()
{
    throw {x:1,y:2};
}

function throwNode()
{
    throw document.body;
}

function catcher(func)
{
    try {
        func();
    } catch (e) {
        console.log("CATCHER: " + e);
    }
}

function nestedCatchBlocks()
{
    try {
        throw "outer exception";
    } catch (e1) {
        try {
            throw "inner exception";
        } catch (e2) {
            console.log(e2);
        }
        console.log(e1);
    }

    TestPage.dispatchEventToFrontend("AfterTestFunction");
}
