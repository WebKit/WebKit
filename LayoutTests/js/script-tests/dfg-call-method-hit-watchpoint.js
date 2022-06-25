description(
"Tests correctness of method calls when the prototype is changed."
);

function Thingy() {
}

Thingy.prototype = {
    foo: function() {
        return 42;
    }
};

function callFoo(o) {
    return o.foo();
}

var o = new Thingy();

silentTestPass = true;
noInline(callFoo);

for (var i = 0; i < 200; i = dfgIncrement({f:callFoo, i:i + 1, n:100})) {
    if (i == 150)
        Thingy.prototype.foo = function() { return 56; }
    var expected;
    if (i < 150)
        expected = 42;
    else
        expected = 56;
    shouldBe("callFoo(o)", "" + expected);
}

