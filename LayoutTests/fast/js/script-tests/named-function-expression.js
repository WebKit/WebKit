description(
"Bug 4698 - kjs does not allow named functions in function expressions."
);

function Call(lambda) { return lambda(); }

debug("anonymous function expression");
shouldBe("var x = (function(a,b){ return a + b; }); x(1,2)", "3");

debug("named function expression");
shouldBe("var x = (function Named(a,b){ return a + b; }); x(2,3)", "5");

debug("eval'd code should be able to access scoped variables");
shouldBe("var z = 6; var x = eval('(function(a,b){ return a + b + z; })'); x(3,4)", "13");

debug("eval'd code + self-check");
shouldBe("var z = 10; var x = eval('(function Named(a,b){ return (!!Named) ? (a + b + z) : -999; })'); x(4,5)", "19");

debug("named function expressions are not saved in the current context");
shouldBe('(function Foo(){ return 1; }); try { Foo(); throw "FuncExpr was stored"; } catch(e) { if(typeof(e)=="string") throw e; } 1', "1");

debug("recursion is possible, though");
shouldBe("var ctr = 3; var x = (function Named(a,b){ if(--ctr) return 2 * Named(a,b); else return a + b; }); x(5,6)", "44");

debug("regression test where kjs regarded an anonymous function declaration (which is illegal) as a FunctionExpr");
shouldBe('try { eval("function(){ return 2; };"); return 0; } catch(e) { 1; }', "1");

var successfullyParsed = true;
