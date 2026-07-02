description(
"Tests that logical not of an object where it is predicted either final object or other (i.e. null or undefined) performs correctly when document.all is present."
);

if (document.all)
    var unused = 1;

function foo(a) {
    var t = !a;

    if (a == 16)
        return -1;

    if (t)
        return false;
    return true;
}

for (var i = 0; i < 100; ++i) {
    if (i%2) {
        var o = {f:42};
        shouldBe("foo(o)", "true");
    } else
        shouldBe("foo(null)", "false");
}

