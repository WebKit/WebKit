description(
"Tests that passing the global object to an array access that will arrayify to ArrayWithArrayStorage doesn't break things."
);

function foo(array) {
    var result = 0;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}

noInline(foo);

var array = [1, 2, 3];
while (!dfgCompiled({f:foo}))
    foo(array);

array = [1, , 3];
array.__defineGetter__(1, function() { return 6; });
while (!dfgCompiled({f:foo, compiles:2}))
    foo(array);

var w = this;
w[0] = 1;
w.length = 1;
shouldBe("foo(w)", "1");
