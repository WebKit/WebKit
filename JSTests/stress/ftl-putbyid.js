
Object.prototype.__defineSetter__("r", function(val){ o = val })

function foo(x){
    var n = { }
    n.r = x;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i)
    foo(i);

