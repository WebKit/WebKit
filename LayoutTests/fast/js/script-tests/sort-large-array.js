description("This tests sorting an array with more than 10,000 values.");

var test = [];
for (var i = 0; i < 10010; i++)
    test.push(10009 - i);
test.sort(function(a, b) {return a - b;});

shouldBe("test.length", "10010");
shouldBe("test[9999]", "9999");
shouldBe("test[10000]", "10000");
shouldBe("test.slice(0, 20).join(', ')", "'0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19'");
shouldBe("test.slice(9990, 10010).join(', ')", "'9990, 9991, 9992, 9993, 9994, 9995, 9996, 9997, 9998, 9999, 10000, 10001, 10002, 10003, 10004, 10005, 10006, 10007, 10008, 10009'");

var testNoValues = [];
testNoValues.length = 10110;
testNoValues.sort(function(a, b) {return a < b;});

shouldBe("testNoValues.length", "10110");
shouldBe("testNoValues[9999]", "undefined");
shouldBe("testNoValues[10000]", "undefined");

var successfullyParsed = true;
