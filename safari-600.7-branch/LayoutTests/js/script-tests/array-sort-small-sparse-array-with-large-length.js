description(
"Tests that we can quickly sort a small sparse array that has a large length."
);

var array = [];
array[10000000] = 42;
array.sort();
var result = "";
for (var s in array) {
    debug("array[" + s + "] = " + array[s]);
    if (result)
        result += ",";
    result += array[s];
}
shouldBe("result", "\"42\"");
shouldBe("array.length", "10000001");
shouldBe("array[0]", "42");
