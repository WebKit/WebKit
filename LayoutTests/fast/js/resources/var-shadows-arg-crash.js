description(
'Tests to ensure that activations are built correctly in the face of duplicate parameter names and do not cause crashes.'
);


function test(a) {
    var b, a = "success";
    return function() {
        return a;
    }
}

shouldBe('test()()', '"success"');

var successfullyParsed = true;
