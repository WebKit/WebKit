function f(a,b,c) {
   var d, e;
   var args = "";
   for (var i = 0; i < arguments.length; i++)
       args+=arguments[i]+ ((i == arguments.length - 1) ? "" : ", ");
   return args;
}
var a = 0;
var b = 0;
var c = 0;
var d = 0;
shouldBe('eval("f()")', '""');
shouldBe('eval("f(1)")', '"1"');
shouldBe('eval("f(1, 2)")', '"1, 2"');
shouldBe('eval("f(1, 2, 3)")', '"1, 2, 3"');
shouldBe('eval("f(1, 2, 3, 4)")', '"1, 2, 3, 4"');
shouldBe('eval("f(1, 2, 3, 4, 5)")', '"1, 2, 3, 4, 5"');
shouldBe('eval("f(1, 2, 3, 4, 5, 6)")', '"1, 2, 3, 4, 5, 6"');
successfullyParsed = true;