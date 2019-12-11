function testArrayConcat() {
    var array = 'abc'.match(/(a)(b)(c)/);
    var result = array.concat();
    var expectedResult = ["abc", "a", "b", "c"];

    if (result.length != 4)
        throw new Error("Runtime array length is incorrect");
    for (var i = 0; i < result.length; i++) {
        if (result[i] != expectedResult[i])
            throw new Error("Runtime array concat result is incorrect");
    }
};

testArrayConcat();
