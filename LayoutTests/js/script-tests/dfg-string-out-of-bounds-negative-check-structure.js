description(
"Tests what happens when you do a negative out-of-bounds access on a string and use that to install a getter that clobbers a structure."
);

function foo(s, o) {
    var x = o.f;
    s[-1];
    var y = o.g;
    return x + y;
}

noInline(foo);
silentTestPass = true;

var theObject = {};

String.prototype.__defineGetter__("-1", function() { delete theObject.g; });

while (testRunner.numberOfDFGCompiles(foo) < 1)
    shouldBe("foo(\"hello\", {f:1, g:2})", "3");

theObject = {f:1, g:2};
shouldBe("foo(\"hello\", theObject)", "0/0");
