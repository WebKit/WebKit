function foo(f) {
    if ($vm.dfgTrue())
        f = bar;
    return f().f;
}

noInline(foo);

var object;
function bar() {
    return object;
}

function baz() { return {f:42}; };

object = {f:42};
for (var i = 0; i < 1000; ++i)
    foo((i & 1) ? bar : baz);

object = {e:1, f:2};
var result = foo(bar);
if (result != 2)
    throw "Error: bad result: " + result;

