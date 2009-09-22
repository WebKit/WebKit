description(
'Tests to ensure that activations are built correctly in the face of duplicate parameter names and do not cause crashes.'
);

function test1(a, b, b, b, b, b, b) {
    return function() {
        return a;
    }
}

shouldBe('test1("success")()', '"success"');

function test2(a, a, a, a, a, a, b) {
    return function() {
        return b;
    }
}

shouldBe('test2("success", "success", "success", "success", "success", "success", "success")()', '"success"');

var successfullyParsed = true;
