description(
'Tests whether bytecode codegen properly handles temporaries across multiple global blocks.'
);

var v1 = 1;
v1 += assign1();
shouldBe("v1", "2");

var o2 = { a: 1 };
var v2 = o2;
v2.a = assign2();
shouldBe("o2.a", "2");

var o3 = { a: 1 };
var v3 = o3;
v3.a += assign3();
shouldBe("o3.a", "2");

var v4 = { a: 1 };
var r4 = v4[assign4()];
shouldBe("r4", "1");

var o5 = { a: 1 };
var v5 = o5;
v5[assign5()] = 2;
shouldBe("o5.a", "2");

var o6 = { a: 1 };
var v6 = o6;
v6["a"] = assign6();
shouldBe("o6.a", "2");

var o7 = { a: 1 };
var v7 = o7;
v7[assign7()] += 1;
shouldBe("o7.a", "2");

var o8 = { a: 1 };
var v8 = o8;
v8["a"] += assign8();
shouldBe("o8.a", "2");

var successfullyParsed = true;
