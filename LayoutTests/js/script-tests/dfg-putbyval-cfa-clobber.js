description(
"Tests that CFA knows that PutByVal can clobber the world."
);

var theBar;

function foo(a, b) {
    var x;
    function bar() {
        x = 57;
    }
    theBar = bar;
    x = 42;
    a[b] = 73;
    return x;
}

array = [];
array.__defineSetter__("-1", function(v) { theBar(); });

for (var i = 0; i < 200; ++i)
    shouldBe("foo(array, -1)", "57");
