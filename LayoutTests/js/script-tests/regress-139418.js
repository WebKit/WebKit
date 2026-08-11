description(
"Regression test for https://webkit.org/b/139418."
);

function InnerObjectNoSetter()
{
    this._enabled = false;
}

InnerObjectNoSetter.prototype = {
    get enabled()
    {
        return this._enabled;
    }
}

function StrictOuterObject(inner)
{
    this._innerObject = inner;
}

StrictOuterObject.prototype = {
    get enabled()
    {
        "use strict";
        return this._innerObject.enabled;
    },

    set enabled(x)
    {
        "use strict";
        this._innerObject.enabled = x;
    }
}

var innerNoSetter = new InnerObjectNoSetter;
var strictOuterNoInnerSetter = new StrictOuterObject(innerNoSetter);

for (var i = 0; i < 1000; ++i) {
    var  noExceptionWithMissingSetter = "Missing setter called with strict mode should throw exception and didn't!";
    try {
        strictOuterNoInnerSetter.enabled = true;
        throw  noExceptionWithMissingSetter;
    } catch (e) {
        if (e instanceof TypeError)
            ; // This is the expected exception
        else if (!((e instanceof String) && (e ==  noExceptionWithMissingSetter)))
            throw e // rethrow "missing exception" exception
        else
            throw "Missing setter called with strict mode threw wrong exception: " + e;
    }
    if (strictOuterNoInnerSetter.enabled)
        throw "Setter unexpectedly modified value";
}
