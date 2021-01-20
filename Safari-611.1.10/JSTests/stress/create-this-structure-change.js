var globalO;

function Foo()
{
    this.g = 42;
}

class RealBar extends Foo {
    constructor()
    {
        var o = globalO;
        var result = o.f;
        super();
        result += o.f;
        this.result = result;
    }
}

var doIntercept = false;
var didExecuteFGetter = false;
var Bar = new Proxy(RealBar, {
    get: function(target, property, receiver) {
        if (property == "prototype" && doIntercept) {
            globalO.__defineGetter__("f", function() {
                didExecuteFGetter = true;
                return 666;
            });
        }
        return Reflect.get(target, property, receiver);
    }
});

noInline(RealBar);

for (var i = 0; i < 10000; ++i) {
    (function() {
        globalO = {f:43};
        var result = new Bar().result;
        if (result != 86)
            throw "bad result in loop: " + result;
    })();
}

doIntercept = true;
globalO = {f:43};
var result = new Bar().result;
if (result != 709)
    throw "bad result at end: " + result;
if (!didExecuteFGetter)
    throw "did not execute f getter";

