description(
"Regression test for https://webkit.org/b/139229. This test should not crash."
);

function InnerObjectNoGetter()
{
    this._enabled = false;
}

InnerObjectNoGetter.prototype = {
    set enabled(x)
    {
        this._enabled = x;
    }
}

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

function OuterObject(inner)
{
    this._innerObject = inner;
}

OuterObject.prototype = {
    get enabled()
    {
        return this._innerObject.enabled;
    },

    set enabled(x)
    {
        this._innerObject.enabled = x;
    }
}

var count = 0;

var innerNoGetter = new InnerObjectNoGetter;
var outerNoInnerGetter = new OuterObject(innerNoGetter);

for (var i = 0; i < 1000; ++i) {
    if (outerNoInnerGetter.enabled)
        ++count;
}

var innerNoSetter = new InnerObjectNoSetter;
var outerNoInnerSetter = new OuterObject(innerNoSetter);

for (var i = 0; i < 1000; ++i) {
    outerNoInnerSetter.enabled = true;
    if (outerNoInnerSetter.enabled)
        ++count;
}

if (count)
    throw "Error: bad result: count should be 0 but was: " + count;
