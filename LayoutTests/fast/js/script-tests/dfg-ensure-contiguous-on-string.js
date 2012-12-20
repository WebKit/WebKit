description(
"Checks that trying to arrayify a string doesn't crash."
);

function foo(array) {
    var result = 0;
    for (var i = 0; i < array.length; ++i)
        result += array[i];
    return result;
}

var array = [1, 2, 3];
for (var i = 0; i < 200; ++i)
    shouldBe("foo(array)", "6");

array = [1, false, 3];
for (var i = 0; i < 1000; ++i)
    shouldBe("foo(array)", "4");

shouldBe("foo(\"hello\")", "\"0hello\"");
