description(
"Tests what happens if you post-inc and then OSR exit."
);

function foo(o, i) {
    o.f.f = i++;
}

for (var i = 0; i < 100; ++i) {
    var o = {};
    if (i == 99)
        o.f = 42;
    foo({f:o}, 0);
    shouldBe("o.f", "0");
}
