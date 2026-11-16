//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

{
    var using = 42;
    shouldBe(using, 42);
}

{
    var using = function() { return 1; };
    shouldBe(using(), 1);
}

{
    let using = 10;
    shouldBe(using, 10);
}

{
    function using() { return "fn"; }
    shouldBe(using(), "fn");
}

{
    let obj = { using: 5 };
    shouldBe(obj.using, 5);
}

{
    let using = [1, 2, 3];
    shouldBe(using.length, 3);
}

shouldBe(eval("var using = 99; using"), 99);

shouldBe(eval("using\n= 42"), 42);
