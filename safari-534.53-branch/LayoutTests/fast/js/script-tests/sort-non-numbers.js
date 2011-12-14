description("This tests numerically sorting an array of non-numbers.");

var test = [ "2", "1", "3" ];
test.sort(function (v1, v2) {
    return v1 - v2;
});

shouldBe("String(test)", "'1,2,3'");

var successfullyParsed = true;
