description(
"This tests that common subexpression elimination knows to be conservative when getters are involved."
);

var array = [1, 2, 3];

var counter = 2;

array.__defineGetter__("foo", function() {
    counter++;
    array[1] = counter;
    return 72;
});

function doSomeGets(array) {
    // This code tries to be evil by asking for array.foo using the by-val syntax. The DFG had
    // a bug where it was incorrectly modeling the heap in its common subexpression phase, when
    // encountering a GetByVal while doing a GetByVal load elimination. Essentially, when trying
    // to find a load redundant with array[1], it would notice array["foo"] and only reject it
    // for CSE without stopping its search; it would then find the first array[1] and do the
    // elimination. That's clearly wrong since array["foo"] does not speculate that it does not
    // have getters; in this case it does and that getter clobbers array[1].
    return array[1] + array["foo"] + array[1];
}

for (var i = 0; i < 1000; ++i) {
    shouldBe("doSomeGets(array)", "" + ((i + 2) + 72 + (i + 2 + 1)));
    shouldBe("counter", "" + (2 + i + 1));
    shouldBe("array[1]", "" + (2 + i + 1));
}

