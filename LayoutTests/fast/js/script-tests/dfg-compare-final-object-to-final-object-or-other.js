description(
"Tests that CompareEq for the case where one side is predicted final object and the other side is predicted either final object or other (i.e. null or undefined) when document.all is present."
);

if (document.all)
    var unused = 1;

function foo(a, b) {
    var result = false;
    var eq = (a == b);

    if (b == 42)
	return -1;

    if (eq)
        result = true;
    return result;
}

for (var i = 0; i < 100; ++i) {
    if (i%2) {
        var o = {f:42};
        shouldBe("foo(o, o)", "true");
    } else
        shouldBe("foo({f:42}, null)", "false");
}

