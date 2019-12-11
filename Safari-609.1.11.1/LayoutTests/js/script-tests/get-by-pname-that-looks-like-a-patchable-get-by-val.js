description(
"Tests that using get_by_pname in a way that appears like a get_by_val that can be patched does not cause the patching machinery to crash."
);

function foo() {
    var o = [1, 2, 3];
    var result = 0;
    
    for (var i = 0; i < 100; ++i) {
        for (var s in o) {
            s = 0;
            result += o[s];
        }
    }
    
    return result;
}

shouldBe("foo()", "300");


