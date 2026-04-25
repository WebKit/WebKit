description(
"Tests that an Int32ToDouble placed on a SetLocal does a forward exit, and works right even when the relevant variable is actually predicted numeric."
);

var counter = 0;

function checkpoint(text) {
    debug("Checkpoint: " + text);
    counter++;
}

function func1() {
    checkpoint("a");
    a = Date.now() + Date.now() + Date.now() + Date.now() + Date.now() + Date.now();
    checkpoint("b");
    if (counter < 1100)
        return 0;
}
function func2() {
    checkpoint("c");
    return Date.now() + Date.now() + Date.now() + Date.now() + Date.now() + Date.now();
}

function func3(s) {
    checkpoint("1");
    s = func1(); // The bug is that this function will be called twice, if our Int32ToDouble hoisting does a backward speculation.
    checkpoint("2");
    s = func2();
    checkpoint("3");
    return s;
}

function test() {
    return func3(1);
}


for (var i=0; i < 200; i++) {
    test();
}

shouldBe("counter", "1200");
