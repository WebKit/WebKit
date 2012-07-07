description(
"This tests that op_get_by_pname is compiled correctly."
);

function foo(o) {
    var result = 0;
    for (var n in o)
        result += o[n];
    return result;
}

var o = {a:1, b:3, c:7};
var p = {a:1, b:2, c:3, d:4};
var q = {a:1, b:2, c:3, d:4, e:3457};
var r = {a:1, b:2, c:3, d:4, e:91, f:12};
var s = {a:1, b:2, c:3, d:4, e:91, f:12, g:69};

for (var i = 0; i < 100; ++i) {
    shouldBe("foo(o)", "11");
    shouldBe("foo(p)", "10");
    shouldBe("foo(q)", "3467");
    shouldBe("foo(r)", "113");
    shouldBe("foo(s)", "182");
}

