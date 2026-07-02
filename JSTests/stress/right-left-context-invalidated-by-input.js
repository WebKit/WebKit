//@ runDefault

function test(when)
{
    /bar/.exec("foo bar baz");
    
    function validateContexts(when)
    {
        if (RegExp.leftContext !== "foo ")
            throw "Error: " + when + ": bad leftContext: " + RegExp.leftContext;
        if (RegExp.rightContext !== " baz")
            throw "Error: " + when + ": bad rightContext: " + RegExp.rightContext;
    }

    if (when === "before")
        validateContexts("before");
    
    RegExp.input = "";
    
    if (when === "after")
        validateContexts("after");
}

test("before");
test("after");
