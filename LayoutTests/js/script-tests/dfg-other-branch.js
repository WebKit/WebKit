description(
"This tests that speculating other on a branch does not corrupt state."
);

function foo(a) {
    if (a.f)
        return "yes";
    else
        return "no";
}

function bar(a) {
    return !a.f;
}

for (var i = 0; i < 100; ++i)
    foo({f:void(0)});

for (var i = 0; i < 10; ++i)
    shouldBe("foo({f:i})", i ? "\"yes\"" : "\"no\"");

for (var i = 0; i < 100; ++i)
    bar({f:void(0)});

for (var i = 0; i < 10; ++i)
    shouldBe("bar({f:i})", i ? "false" : "true");

var successfullyParsed = true;
