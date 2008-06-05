description(
'Tests whether bytecode codegen properly handles temporaries.'
);

var a = true;
a = false || a;
shouldBeTrue("a");

var b = false;
b = true && b;
shouldBeFalse("b");

function TestObject() {
    this.toString = function() { return this.test; }
    this.test = "FAIL";
    return this;
}

function assign_test1()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = "PASS";
    return testObject.test;
}

shouldBe("assign_test1()", "'PASS'");

function assign_test2()
{
    var testObject = new TestObject;
    var a = testObject;
    a = a.test = "PASS";
    return testObject.test;
}

shouldBe("assign_test2()", "'PASS'");

function assign_test3()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = a = "PASS";
    return testObject.test;
}

shouldBe("assign_test3()", "'PASS'");

var testObject4 = new TestObject;
var a4 = testObject4;
a4.test = this.a4 = "PASS";

shouldBe("testObject4.test", "'PASS'");

var testObject5 = new TestObject;
var a5 = testObject5;
a5 = this.a5.test = "PASS";

shouldBe("testObject5.test", "'PASS'");

function assign_test6()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test6()", "'PASS'");

function assign_test7()
{
    var testObject = new TestObject;
    var a = testObject;
    a = a["test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test7()", "'PASS'");

function assign_test8()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = a = "PASS";
    return testObject.test;
}

shouldBe("assign_test8()", "'PASS'");

function assign_test9()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = this.a = "PASS";
    return testObject.test;
}

shouldBe("assign_test9()", "'PASS'");

var testObject10 = new TestObject;
var a10 = testObject10;
a10 = this.a10["test"] = "PASS";

shouldBe("testObject10.test", "'PASS'");

function assign_test11()
{
    var testObject = new TestObject;
    var a = testObject;
    a[a = "test"] = "PASS";
    return testObject.test;
}

shouldBe("assign_test11()", "'PASS'");

function assign_test12()
{
    var test = "test";
    var testObject = new TestObject;
    var a = testObject;
    a[test] = "PASS";
    return testObject.test;
}

shouldBe("assign_test12()", "'PASS'");

function assign_test13()
{
    var testObject = new TestObject;
    var a = testObject;
    a.test = (a = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test13()", "'PASS'");

function assign_test14()
{
    var testObject = new TestObject;
    var a = testObject;
    a["test"] = (a = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test14()", "'PASS'");

function assign_test15()
{
    var test = "test";
    var testObject = new TestObject;
    var a = testObject;
    a[test] = (test = "FAIL", "PASS");
    return testObject.test;
}

shouldBe("assign_test15()", "'PASS'");

function assign_test16()
{
    var a = 1;
    a = (a = 2);
    return a;
}

shouldBe("assign_test16()", "2");

var a17 = 1;
a17 += (a17 += 1);

shouldBe("a17", "3");

function assign_test18()
{
    var a = 1;
    a += (a += 1);
    return a;
}

shouldBe("assign_test18()", "3");

var a19 = { b: 1 };
a19.b += (a19.b += 1);

shouldBe("a19.b", "3");

function assign_test20()
{
    var a = { b: 1 };
    a.b += (a.b += 1);
    return a.b;
}

shouldBe("assign_test20()", "3");

var a21 = { b: 1 };
a21["b"] += (a21["b"] += 1);

shouldBe("a21['b']", "3");

function assign_test22()
{
    var a = { b: 1 };
    a["b"] += (a["b"] += 1);
    return a["b"];
}

shouldBe("assign_test22()", "3");

function assign_test23()
{
    var o = { b: 1 };
    var a = o;
    a.b += a = 2;
    return o.b;
}

shouldBe("assign_test23()", "3");

function assign_test24()
{
    var o = { b: 1 };
    var a = o;
    a["b"] += a = 2;
    return o["b"];
}

shouldBe("assign_test24()", "3");

function assign_test25()
{
    var o = { b: 1 };
    var a = o;
    a[a = "b"] += a = 2;
    return o["b"];
}

shouldBe("assign_test25()", "3");

function assign_test26()
{
    var o = { b: 1 };
    var a = o;
    var b = "b";
    a[b] += a = 2;
    return o["b"];
}

shouldBe("assign_test26()", "3");

function assign_test27()
{
    var o = { b: 1 };
    var a = o;
    a.b += (a = 100, 2);
    return o.b;
}

shouldBe("assign_test27()", "3");

function assign_test28()
{
    var o = { b: 1 };
    var a = o;
    a["b"] += (a = 100, 2);
    return o["b"];
}

shouldBe("assign_test28()", "3");

function assign_test29()
{
    var o = { b: 1 };
    var a = o;
    var b = "b";
    a[b] += (a = 100, 2);
    return o["b"];
}

shouldBe("assign_test29()", "3");

function assign_test30()
{
    var a = "foo";
    a += (a++);
    return a;
}

shouldBe("assign_test30()", "'fooNaN'");

function assign_test31()
{
    function result() { return "PASS"; }
    return (globalVar = result)()
}

shouldBe("assign_test31()", "'PASS'");

function bracket_test1()
{
    var o = [-1];
    var a = o[++o];
    return a;
}

shouldBe("bracket_test1()", "-1");

function bracket_test2()
{
    var o = [1];
    var a = o[--o];
    return a;
}

shouldBe("bracket_test2()", "1");

function bracket_test3()
{
    var o = [0];
    var a = o[o++];
    return a;
}

shouldBe("bracket_test3()", "0");

function bracket_test4()
{
    var o = [0];
    var a = o[o--];
    return a;
}

shouldBe("bracket_test4()", "0");

function bracket_test5()
{
    var o = [1];
    var a = o[o ^= 1];
    return a;
}

shouldBe("bracket_test5()", "1");

function bracket_test6()
{
    var o = { b: 1 }
    var b = o[o = { b: 2 }, "b"];
    return b;
}

shouldBe("bracket_test6()", "1");

successfullyParsed = true;
