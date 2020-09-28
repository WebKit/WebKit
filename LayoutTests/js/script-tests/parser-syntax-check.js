description(
"This test checks that the following expressions or statements are valid ECMASCRIPT code or should throw parse error"
);

function testFailed(msg) {
    description(msg);
    throw new Error("Bad!");
}

function runTest(_a, expectSyntaxError)
{
    var error;

    if (typeof _a != "string")
        testFailed("runTest expects string argument: " + _a);
    try {
        eval(_a);
    } catch (e) {
        error = e;
    }

    if (expectSyntaxError) {
        if (error && error instanceof SyntaxError)
            testPassed(`Invalid: "${_a}". Produced the following syntax error: "${error.toString()}"`);
        else if (error)
            testFailed('Invalid: "' + _a + '" should throw SyntaxError but got ' + (error.name || error));
        else
            testFailed('Invalid: "' + _a + '" but did not throw');
    } else {
        if (!error)
            testPassed('Valid:   "' + _a + '"');
        else if (!(error instanceof SyntaxError))
            testPassed('Valid:   "' + _a + '" with ' + (error.name || error));
        else
            testFailed('Valid:   "' + _a + '" should NOT throw but got ' + (error.name || error));
    }
}

function valid(_a)
{
    // Test both the grammar and the syntax checker
    runTest(_a, false);
    runTest("function f() { " + _a + " }", false);
}

function onlyValidGlobally(_a)
{
    runTest(_a, false);
    runTest("function f() { " + _a + " }", true);
}

function onlyInvalidGlobally(_a)
{
    runTest(_a, true);
    runTest("function f() { " + _a + " }", false);
}

function invalid(_a)
{
    // Test both the grammar and the syntax checker
    runTest(_a, true);
    runTest("function f() { " + _a + " }", true);
}

// known issue:
//   some statements requires statement as argument, and
//   it seems the End-Of-File terminator is converted to semicolon
//      "a:[EOF]" is not parse error, while "{ a: }" is parse error
//      "if (a)[EOF]" is not parse error, while "{ if (a) }" is parse error
// known issues of bison parser:
//   accepts: 'function f() { return 6 + }' (only inside a function declaration)
//   some comma expressions: see reparsing-semicolon-insertion.js

debug  ("Unary operators and member access");

valid  ("");
invalid("(a");
invalid("a[5");
invalid("a[5 + 6");
invalid("a.");
invalid("()");
invalid("a.'l'");
valid  ("a: +~!new a");
invalid("new -a");
valid  ("new (-1)")
invalid("a: b: c: new f(x++)++")
valid  ("(a)++");
invalid("(1--).x");
invalid("a-- ++");
invalid("(a:) --b");
invalid("++ -- ++ a");
invalid("++ new new a ++");
valid  ("delete void 0");
invalid("delete the void");
invalid("(a++");
invalid("++a--");
invalid("++((a))--");
invalid("(a.x++)++");
invalid("1: null");
invalid("+-!~");
invalid("+-!~((");
invalid("a)");
invalid("a]");
invalid(".l");
invalid("1.l");
valid  ("1 .l");

debug ("Octal numbers");

valid  ("'use strict'; 0");
valid  ("0");
invalid("'use strict'; 00");
valid  ("00");
invalid("'use strict'; 08");
valid  ("08");
invalid("'use strict'; 09");
valid  ("09");

debug  ("Binary and conditional operators");

valid  ("a + + typeof this");
invalid("a + * b");
invalid("a ? b");
invalid("a ? b :");
invalid("%a");
invalid("a-");
valid  ("a = b ? b = c : d = e");
valid  ("s: a[1].l ? b.l['s'] ? c++ : d : true");
valid  ("a ? b + 1 ? c + 3 * d.l : d[5][6] : e");
valid  ("a in b instanceof delete -c");
invalid("a in instanceof b.l");
valid  ("- - true % 5");
invalid("- false = 3");
invalid("a: b: c: (1 + null) = 3");
valid  ("a[2] = b.l += c /= 4 * 7 ^ !6");
invalid("a + typeof b += c in d");
invalid("typeof a &= typeof b");
invalid("a: ((typeof (a))) >>>= a || b.l && c");
valid  ("a: b: c[a /= f[a %= b]].l[c[x] = 7] -= a ? b <<= f : g");
valid  ("-void+x['y'].l == x.l != 5 - f[7]");

debug  ("Function calls (and new with arguments)");

valid  ("a()()()");
invalid("s: l: a[2](4 == 6, 5 = 6)(f[4], 6)");
valid  ("s: eval(a.apply(), b.call(c[5] - f[7]))");
invalid("a(");
invalid("a(5");
invalid("a(5,");
valid("a(5,)");
invalid("a(5,6");
valid  ("a(b[7], c <d> e.l, new a() > b)");
invalid("a(b[5)");
invalid("a(b.)");
valid  ("~new new a(1)(i++)(c[l])");
invalid("a(*a)");
valid  ("((((a))((b)()).l))()");
valid  ("(a)[b + (c) / (d())].l--");
valid  ("new (5)");
invalid("new a(5");
valid  ("new (f + 5)(6, (g)() - 'l'() - true(false))");
invalid("a(.length)");

debug  ("function declaration and expression");

valid  ("function f() {}");
valid  ("function f(a,b) {}");
invalid("function () {}");
invalid("function f(a b) {}");
valid("function f(a,) {}");
invalid("function f(a,");
invalid("function f(a, 1) {}");
valid  ("function g(arguments, eval) {}");
valid  ("function f() {} + function g() {}");
invalid("(function a{})");
invalid("(function this(){})");
valid  ("(delete new function f(){} + function(a,b){}(5)(6))");
valid  ("6 - function (m) { function g() {} }");
invalid("function l() {");
invalid("function l++(){}");

debug  ("Array and object literal, comma operator");

// Note these are tested elsewhere, no need to repeat those tests here
valid  ("[] in [5,6] * [,5,] / [,,5,,] || [a,] && new [,b] % [,,]");
invalid("[5,");
invalid("[,");
invalid("(a,)");
valid  ("1 + {get get(){}, set set(a){}, get1:4, set1:get-set, }");
invalid("1 + {a");
invalid("1 + {a:");
invalid("1 + {get l(");
invalid(",a");
valid  ("(4,(5,a(3,4))),f[4,a-6]");
invalid("(,f)");
invalid("a,,b");
invalid("a ? b, c : d");

debug  ("simple statements");

valid  ("{ }");
invalid("{ { }");
valid  ("{ ; ; ; }");
valid  ("a: { ; }");
invalid("{ a: }");
valid  ("{} f; { 6 + f() }");
valid  ("{ a[5],6; {} ++b-new (-5)() } c().l++");
valid  ("{ l1: l2: l3: { this } a = 32 ; { i++ ; { { { } } ++i } } }");
valid  ("if (a) ;");
invalid("{ if (a) }");
invalid("if a {}");
invalid("if (a");
invalid("if (a { }");
valid  ("x: s: if (a) ; else b");
invalid("else {}");
valid  ("if (a) if (b) y; else {} else ;");
invalid("if (a) {} else x; else");
invalid("if (a) { else }");
valid  ("if (a.l + new b()) 4 + 5 - f()");
valid  ("if (a) with (x) ; else with (y) ;");
invalid("with a.b { }");
valid  ("while (a() - new b) ;");
invalid("while a {}");
valid  ("do ; while(0) i++"); // Is this REALLY valid? (Firefox also accepts this)
valid  ("do if (a) x; else y; while(z)");
invalid("do g; while 4");
invalid("do g; while ((4)");
valid  ("{ { do do do ; while(0) while(0) while(0) } }");
valid  ("do while (0) if (a) {} else y; while(0)");
valid  ("if (a) while (b) if (c) with(d) {} else e; else f");
invalid("break ; break your_limits ; continue ; continue living ; debugger");
invalid("debugger X");
invalid("break 0.2");
invalid("continue a++");
invalid("continue (my_friend)");
valid  ("while (1) break");
valid  ("do if (a) with (b) continue; else debugger; while (false)");
invalid("do if (a) while (false) else debugger");
invalid("while if (a) ;");
valid  ("if (a) function f() {} else function g() {}");
invalid("if (a()) while(0) function f() {} else function g() {}");
invalid("if (a()) function f() { else function g() }");
invalid("if (a) if (b) ; else function f {}");
invalid("if (a) if (b) ; else function (){}");
valid  ("throw a");
valid  ("throw a + b in void c");
invalid("throw");

debug  ("var and const statements");

valid  ("var a, b = null");
valid  ("const a = 5, b = 10, c = 20");
invalid("var");
invalid("var = 7");
invalid("var c (6)");
valid  ("if (a) var a,b; else { const b = 1, c = 2; }");
invalid("if (a) var a,b; else { const b, c }");
invalid("var 5 = 6");
valid  ("while (0) var a, b, c=6, d, e, f=5*6, g=f*h, h");
invalid("var a = if (b) { c }");
invalid("var a = var b");
invalid("const a = b += c, a, a, a = (b - f())");
invalid("var a %= b | 5");
invalid("var (a) = 5");
invalid("var a = (4, b = 6");
invalid("const 'l' = 3");
invalid("var var = 3");
valid  ("var varr = 3 in 1");
valid  ("const  a = void 7 - typeof 8, b = 8");
invalid("const a, a, a = void 7 - typeof 8, a = 8");
invalid("const x_x = 6 /= 7 ? e : f");
invalid("var a = ?");
invalid("const a = *7");
invalid("var a = :)");
valid  ("var a = a in b in c instanceof d");
invalid("var a = b ? c, b");
invalid("const a = b : c");
valid("const a = 7; eval(''); a++");
valid("const a = 7; eval(''); a--");
valid("const a = 7; with({}) a++");
valid("const a = 7; with({}) a--");
valid("const a = 7; eval(''); a+=1");
valid("const a = 7; eval(''); a-=1");
valid("const a = 7; with({}) a+=1");
valid("const a = 7; with({}) a-=1");
valid("const a = 7; eval(''); a=1");
valid("const a = 7; eval(''); a=1");
valid("const a = 7; with({}) a=1");
valid("const a = 7; with({}) a=1");


debug  ("for statement");

valid  ("for ( ; ; ) { break }");
valid  ("for ( a ; ; ) { break }");
valid  ("for ( ; a ; ) { break }");
valid  ("for ( ; ; a ) { break }");
valid  ("for ( a ; a ; ) break");
valid  ("for ( a ; ; a ) break");
valid  ("for ( ; a ; a ) break");
invalid("for () { }");
invalid("for ( a ) { }");
invalid("for ( ; ) ;");
invalid("for a ; b ; c { }");
invalid("for (a ; { }");
invalid("for ( a ; ) ;");
invalid("for ( ; a ) break");
valid  ("for (var a, b ; ; ) { break } ");
valid  ("for (var a = b, b = a ; ; ) break");
valid  ("for (var a = b, c, d, b = a ; x in b ; ) { break }");
valid  ("for (var a = b, c, d ; ; 1 in a()) break");
invalid("for ( ; var a ; ) break");
invalid("for (const x; ; ) break");
invalid("for (const x = 20, y; ; ) break");
valid  ("for (const x = 20; ; ) break");
valid  ("for (const x of []) break");
valid  ("for (const x in {}) break");
invalid("for (const x = 20, x = 30; ; ) { const x = 20; break; }");
valid  ("for (const x = 20; ; ) { const x = 20; break; }");
valid  ("for (const x of []) { const x = 20; break; }");
valid  ("for (const x in {}) { const x = 20; break; }");
invalid("for (const let = 10; ; ) { break; }");
invalid("for (const let in {}) { break; }");
invalid("for (const let of []) { break; }");
invalid("for (let let = 10; ; ) { break; }");
invalid("for (let let in {}) { break; }");
invalid("for (let let of []) { break; }");
invalid("for ( %a ; ; ) { }");
valid  ("for (a in b) break");
valid  ("for (a() in b) break");
valid  ("for (a().l[4] in b) break");
invalid("for (new a in b in c in d) break");
invalid("for (new new new a in b) break");
invalid("for (delete new a() in b) break");
invalid("for (a * a in b) break");
invalid("for ((a * a) in b) break");
invalid("for (a++ in b) break");
invalid("for ((a++) in b) break");
invalid("for (++a in b) break");
invalid("for ((++a) in b) break");
invalid("for (a, b in c) break");
invalid("for (a,b in c ;;) break");
valid  ("for (a,(b in c) ;;) break");
invalid("for ((a, b) in c) break");
invalid("for (a ? b : c in c) break");
invalid("for ((a ? b : c) in c) break");
valid  ("for (var a in b in c) break");
invalid("for (var a = 5 += 6 in b) break");
valid("for (var a = foo('should be hit') in b) break");
invalid("for (var a += 5 in b) break");
invalid("for (var a = in b) break");
invalid("for (var a, b in b) break");
invalid("for (var a = -6, b in b) break");
invalid("for (var a, b = 8 in b) break");
valid("for (var a = (b in c) in d) break");
invalid("for (var a = (b in c in d) break");
invalid("for (var (a) in b) { }");
invalid("for (var a = 7, b = c < d >= d ; f()[6]++ ; --i()[1]++ ) {}");
invalid("for (var {a} = 20 in b) { }");
invalid("for (var {a} = 20 of b) { }");
invalid("for (var {a} = 20 in b) { }");
valid("for (var i = 20 in b) { }");
invalid("for (var i = 20 of b) { }");
invalid("for (var {i} = 20 of b) { }");
invalid("for (var [i] = 20 of b) { }");
invalid("for (let [i] = 20 of b) { }");
invalid("for (const [i] = 20 of b) { }");
invalid("for (const i = 20 of b) { }");
invalid("for (let i = 20 of b) { }");
invalid("for (let i = 20 in b) { }");
invalid("for (const i = 20 in b) { }");
invalid("for (const {i} = 20 in b) { }");
invalid("for (let {i} = 20 in b) { }");
valid("function x(i) { for (let i in {}) { } }");
valid("function x(i) { for (let i of []) { } }");
valid("function x(i) { for (let i of []) { } }");
valid("function x(i) { for (let i; false; ) { } }");
valid("let f = (i) => { for (let i in {}) { } }");
valid("let f = (i) => { for (let i of []) { } }");
valid("let f = (i) => { for (let i of []) { } }");
valid("let f = (i) => { for (let i; false; ) { } }");
valid("function* x(i) { for (let i in {}) { } }");
valid("function* x(i) { for (let i of []) { } }");
valid("function* x(i) { for (let i of []) { } }");
valid("function* x(i) { for (let i; false; ) { } }");
valid("function x(i) { for (const i in {}) { } }");
valid("function x(i) { for (const i of []) { } }");
valid("function x(i) { for (const i of []) { } }");
valid("function x(i) { for (const i = 20; false; ) { } }");
valid("let f = (i) => { for (const i in {}) { } }");
valid("let f = (i) => { for (const i of []) { } }");
valid("let f = (i) => { for (const i of []) { } }");
valid("let f = (i) => { for (const i = 20; false; ) { } }");
valid("function* x(i) { for (const i in {}) { } }");
valid("function* x(i) { for (const i of []) { } }");
valid("function* x(i) { for (const i of []) { } }");
valid("function* x(i) { for (const i = 20; false; ) { } }");

debug  ("try statement");

invalid("try { break } catch(e) {}");
valid  ("try {} finally { c++ }");
valid  ("try { with (x) { } } catch(e) {} finally { if (a) ; }");
invalid("try {}");
invalid("catch(e) {}");
invalid("finally {}");
invalid("try a; catch(e) {}");
invalid("try {} catch(e) a()");
invalid("try {} finally a()");
invalid("try {} catch(e)");
invalid("try {} finally");
invalid("try {} finally {} catch(e) {}");
invalid("try {} catch (...) {}");
valid  ("try {} catch {}");
valid  ("if (a) try {} finally {} else b;");
valid  ("if (--a()) do with(1) try {} catch(ke) { f() ; g() } while (a in b) else {}");
invalid("if (a) try {} else b; catch (e) { }");
invalid("try { finally {}");

debug  ("switch statement");

valid  ("switch (a) {}");
invalid("switch () {}");
invalid("case 5:");
invalid("default:");
invalid("switch (a) b;");
invalid("switch (a) case 3: b;");
valid  ("switch (f()) { case 5 * f(): default: case '6' - 9: ++i }");
invalid("switch (true) { default: case 6: default: }");
invalid("switch (l) { f(); }");
invalid("switch (l) { case 1: ; a: case 5: }");
valid  ("switch (g() - h[5].l) { case 1 + 6: a: b: c: ++f }");
invalid("switch (g) { case 1: a: }");
invalid("switch (g) { case 1: a: default: }");
invalid("switch g { case 1: l() }");
invalid("switch (g) { case 1:");
valid  ("switch (l) { case a = b ? c : d : }");
valid  ("switch (sw) { case a ? b - 7[1] ? [c,,] : d = 6 : { } : }");
invalid("switch (l) { case b ? c : }");
valid  ("switch (l) { case 1: a: with(g) switch (g) { case 2: default: } default: }");
invalid("switch (4 - ) { }");
invalid("switch (l) { default case: 5; }");

invalid("L: L: ;");
invalid("L: L1: L: ;");
invalid("L: L1: L2: L3: L4: L: ;");

invalid("for(var a,b 'this shouldn\'t be allowed' false ; ) ;");
invalid("for(var a,b '");

valid("function __proto__(){}")
valid("(function __proto__(){})")
valid("'use strict'; function __proto__(){}")
valid("'use strict'; (function __proto__(){})")

valid("'use strict'; function f1(a) { function f2(b) { return b; } return f2(a); } f1(5);")
valid("'use strict'; function f1(a) { function f2(b) { function f3(c) { return c; } return f3(b); } return f2(a); } f1(5);")
valid("'use strict'; function f1(a) { if (a) { function f2(b) { return b; } return f2(a); } else return a; } f1(5);")
valid("'use strict'; function f1(a) { function f2(b) { if (b) { function f3(c) { return c; } return f3(b); } else return b; } return f2(a); } f1(5);")
valid("'use strict'; function f1(a) {}; function f1(a) {};")
invalid("'use strict'; { function f1(a) {}; function f1(a) {}; }")
invalid("'use strict'; { let f1; function f1(a) {}; }")
invalid("'use strict'; { function f1(a) {}; let f1; }")
invalid("'use strict'; let f1; function f1(a) {};")
invalid("'use strict'; function f1(a) {}; let f1; ")
invalid("let f1; function f1(a) {};")
valid("function foo() { let f1; { function f1(a) {}; } }")
valid("function foo() { { function f1(a) {}; } let f1; }")
valid("function foo() { { function foo() { }; function foo() { } } }")
invalid("function foo() { 'use strict'; { function foo() { }; function foo() { } } }")
invalid("function foo() { let f1; function f1(a) {}; }")
invalid("let f1; function f1(a) {};")
invalid("{ function f1(a) {}; let f1; }")
invalid("{ function f1(a) {}; const f1 = 25; }")
invalid("{ function f1(a) {}; class f1{}; }")
invalid("function foo() { { let bar; function bar() { } } }")
invalid("function foo() { { function bar() { }; let bar; } }")
invalid("function foo() { { const bar; function bar() { } } }")
invalid("function foo() { { function bar() { }; const bar; } }")
invalid("function foo() { { class bar{}; function bar() { } } }")
invalid("function foo() { { function bar() { }; class bar{}; } }")
valid("switch('foo') { case 1: function foo() {}; break; case 2: function foo() {}; break; }")
invalid("switch('foo') { case 1: let foo; function foo() {}; break; case 2: function foo() {}; break; }")
invalid("switch('foo') { case 1: function foo() {}; let foo; break; case 2: function foo() {}; break; }")
invalid("switch('foo') { case 1: function foo() {}; const foo = 25; break; case 2: function foo() {}; break; }")
invalid("switch('foo') { case 1: function foo() {}; class foo {} ; break; case 2: function foo() {}; break; }")
invalid("switch('foo') { case 1: function foo() {}; break; case 2: function foo() {}; break; case 3: let foo; }")
valid("function foo() { switch('foo') { case 1: function foo() {}; break; case 2: function foo() {}; break; case 3: { let foo; } } }")
invalid("'use strict'; switch('foo') { case 1: function foo() {}; break; case 2: function foo() {}; break; }");
invalid("'use strict'; switch('foo') { case 1: function foo() {}; break; case 2: let foo; break; }");
invalid("'use strict'; switch('foo') { case 1: let foo; break; case 2: function foo() {}; break; }");
valid("'use strict'; switch('foo') { case 1: { let foo; break; } case 2: function foo() {}; break; }");
valid("'use strict'; switch('foo') { case 1: { function foo() { }; break; } case 2: function foo() {}; break; }");
invalid("'use strict'; if (true) function foo() { }; ");
valid("if (true) function foo() { }; ");
valid(" let foo; if (true) function foo() { };");
valid("function baz() { let foo; if (true) function foo() { }; }");
valid("if (true) function foo() { }; let foo;");
valid("{ if (true) function foo() { }; } let foo;");
invalid("let foo; while (false) function foo() { }; ");
invalid("let foo;  { while (false) function foo() { }; } ");
invalid("while (false) function foo() { }; let foo;");
invalid("let foo; while (false) label: function foo() { }; ");
invalid("while (false) label: function foo() { }; let foo;");
invalid("'use strict'; while (false) function foo() { }; ");
invalid("'use strict'; if (false) function foo() { }; ");
invalid("'use strict'; do function foo() { } while (false); ");
invalid("while (false) function foo() { }; ");
valid("if (false) function foo() { }; ");
invalid("do function foo() { } while (false); ");
invalid("if (cond) label: function foo() { }");
invalid("while (true) { while (true) function bar() { } }");
invalid("with ({}) function bar() { }");
valid("function bar() { label: function baz() { } }");
valid("function bar() { let: function baz() { } }");
invalid("function bar() { 'use strict'; let: function baz() { } }");
valid("function bar() { yield: function baz() { } }");
valid("function bar() { label: label2: function baz() { } }");
valid("function bar() { label: label2: label3: function baz() { } }");
invalid("function bar() { label: label2: label weird: function baz() { } }");
valid("function bar() { label: label2: label3: function baz() { } }");
invalid("function bar() { 'use strict'; label: label2: label 3: function baz() { } }");
invalid("function bar() { if (cond) label: function foo() { } }");
invalid("function bar() { while (cond) label: function foo() { } }");
valid("label: function foo() { }");
valid("let: function foo() { }");
valid("yield: function foo() { }");
valid("yield: let: function foo() { }");
invalid("'use strict'; yield: let: function foo() { }");

valid("var str = \"'use strict'; function f1(a) { function f2(b) { return b; } return f2(a); } return f1(arguments[0]);\"; var foo = new Function(str); foo(5);")
valid("var str = \"'use strict'; function f1(a) { function f2(b) { function f3(c) { return c; } return f3(b); } return f2(a); } return f1(arguments[0]);\"; var foo = new Function(str); foo(5);")
valid("var str = \"'use strict'; function f1(a) { if (a) { function f2(b) { return b; } return f2(a); } else return a; } return f1(arguments[0]);\"; var foo = new Function(str); foo(5);")
valid("var str = \"'use strict'; function f1(a) { function f2(b) { if (b) { function f3(c) { return c; } return f3(b); } else return b; } return f2(a); } return f1(arguments[0]);\"; var foo = new Function(str); foo(5);")

valid("if (0) $foo; ")
valid("if (0) _foo; ")
valid("if (0) foo$; ")
valid("if (0) foo_; ")
valid("if (0) obj.$foo; ")
valid("if (0) obj._foo; ")
valid("if (0) obj.foo$; ")
valid("if (0) obj.foo_; ")
valid("if (0) obj.foo\\u03bb; ")
valid("if (0) new a(b+c).d = 5");
invalid("if (0) new a(b+c) = 5");
valid("([1 || 1].a = 1)");
valid("({a: 1 || 1}.a = 1)");

invalid("var a.b = c");
invalid("var a.b;");

valid("for (of of of){}")
valid("for (of; of; of){}")
valid("for (var of of of){}")
valid("for (var of; of; of){}")
invalid("for (var of.of of of){}")
invalid("for (var of[of] of of){}")
valid("for (of.of of of){}")
valid("for (of[of] of of){}")
valid("for (var [of] of of){}")
valid("for (var {of} of of){}")
valid("for (of in of){}")
valid("for (var of in of){}")
invalid("for (var of.of in of){}")
valid("for (of.of in of){}")
valid("for (of[of] in of){}")
invalid("for (var of[of] in of){}")
valid("for (var [of] in of){}")
valid("for (var {of} in of){}")
valid("for ([of] in of){}")
valid("for ({of} in of){}")
invalid("for (var of = x of of){}")
invalid("for (var {of} = x of of){}")
invalid("for (var [of] = x of of){}")


invalid("for (of of of of){}")
invalid("for (of of; of; of){}")
invalid("for (of of []; of; of){}")
invalid("for (of of){}")
invalid("for (var of of){}")
invalid("for (of of in of){}")
invalid("for (of in){}")
invalid("for (var of in){}")

debug("spread operator and destructuring")
valid("foo(...bar)")
valid("o.foo(...bar)")
valid("o[foo](...bar)")
valid("new foo(...bar)")
valid("new o.foo(...bar)")
valid("new o[foo](...bar)")
invalid("foo(...)")
invalid("o.foo(...)")
invalid("o[foo](...)")
invalid("foo(bar...)")
invalid("o.foo(bar...)")
invalid("o[foo](bar...)")
valid("foo(a,...bar)")
valid("o.foo(a,...bar)")
valid("o[foo](a,...bar)")
valid("foo(...bar, a)")
valid("o.foo(...bar, a)")
valid("o[foo](...bar, a)")
valid("[...bar]")
valid("[a, ...bar]")
valid("[...bar, a]")
valid("[...bar,,,,]")
valid("[,,,,...bar]")
valid("({1: x})")
valid("({1: x}=1)")
valid("({1: x}=null)")
valid("({1: x})")
valid("({1: x}=1)")
valid("({1: x}=null)")
valid("({a: b}=null)")
valid("'use strict'; ({1: x})")
valid("'use strict'; ({1: x}=1)")
valid("'use strict'; ({1: x}=null)")
valid("'use strict'; ({a: b}=null)")
valid("var {1:x}=1")
valid("var {x}=1")
valid("var {x, y}=1")
valid("var [x]=1")
valid("var [x, y]=1")
valid("[x]=1")
valid("var [x]=1")
valid("({[x]: 1})")
valid("delete ({a}=1)")
valid("delete ({a:a}=1)")
valid("({a}=1)()")
valid("({a:a}=1)()")
invalid("({a}=1)=1")
invalid("({a:a}=1)=1")
invalid("({a}=1=1)")
invalid("({a:a}=1=1)")
invalid("var {x}")
invalid("var {x, y}")
invalid("var {x} = 20, {x, y}")
invalid("var {foo:bar, bar:baz}")
invalid("var [x]")
invalid("var [x, y]")
invalid("var [x] = [], [x, y]")
valid("({get x(){}})")
valid("({set x(x){}})")
invalid("({get x(a){}})")
invalid("({get x(a,b){}})")
invalid("({set x(){}})")
invalid("({set x(a,b){}})")
valid("({get [x](){}})")
invalid("({get [x (){}})")
invalid("({set [x](){}})")
valid("({set [x](x){}})")
invalid("({set [x (x){}})")
valid("({set foo(x) { } })");
valid("({set foo(x) { 'use strict'; } })");
invalid("({set foo(x = 20) { 'use strict'; } })");
invalid("({set foo({x}) { 'use strict'; } })");
invalid("({set foo([x]) { 'use strict'; } })");
invalid("({set foo(...x) {} })");
valid("class Foo { set v(z) { } }");
valid("class Foo { set v(z) { 'use strict'; } }");
invalid("class Foo { set v(z = 50) { 'use strict'; } }");
invalid("class Foo { set v({z}) { 'use strict'; } }");
invalid("class Foo { set v([z]) { 'use strict'; } }");
invalid("class Foo { set v(...z) { } }");
invalid("class foo { set y([x, y, x]) { } }");
invalid("class foo { set y([x, y, {x}]) { } }");
invalid("class foo { set y({x, x}) { } }");
invalid("class foo { set y({x, field: {x}}) { } }");
valid("class foo { set y({x, field: {xx}}) { } }");
invalid("({[...x]: 1})")
invalid("function f({a, a}) {}");
invalid("function f({a}, a) {}");
invalid("function f([b, b]) {}");
invalid("function f([b], b) {}");
invalid("function f({a: {b}}, b) {}");
valid("function f(a, b = 20) {}");
valid("function f(a = 20, b = a) {}");
valid("function f({a = 20} = {a: 40}, b = a) {}");
valid("function f([a,b,c] = [1,2,3]) {}");
invalid("function f(a, a=20) {}");
invalid("function f({a} = 20, a=20) {}");
invalid("function f([a,b,a] = [1,2,3]) {}");
invalid("function f([a,b,c] = [1,2,3], a) {}");
invalid("function f([a,b,c] = [1,2,3], {a}) {}");
valid("( function(){ return this || eval('this'); }().x = 'y' )");
invalid("function(){ return this || eval('this'); }().x = 'y'");
invalid("1 % +");
invalid("1 % -");
invalid("1 % typeof");
invalid("1 % void");
invalid("1 % !");
invalid("1 % ~");
invalid("1 % delete");
invalid("1 % ++");
invalid("1 % --");
invalid("1 %\n++");
invalid("1 %\n--");
invalid('let {w} = (foo-=()), {} = ("a" ^= "b");');
invalid('const {w} = (foo-=()), {} = ("a" ^= "b");');
invalid('var {w} = (foo-=()), {} = ("a" ^= "b");');
invalid('let {w} = ();');
invalid('let {w} = 1234abc;');
invalid('const {w} = 1234abc;');
invalid('var {w} = 1234abc;');
invalid("var [...x = 20] = 20;");
invalid("var [...[...x = 20]] = 20;");
valid("var [...x] = 20;");
valid("var [...[...x]] = 20;");
valid("var [...[...{x}]] = 20;");
valid("var [...[x = 20, ...y]] = 20;");
valid("var [...[{x} = 20, ...y]] = 20;");
valid("var {x: [y, ...[...[...{z: [...z]}]]]} = 20");
valid("var {x: [y, {z: {z: [...z]}}]} = 20");
invalid("var [...y, ...z] = 20");
valid("var [...{...y}] = 20");
valid("var {a, b, ...r} = {a: 1, b: 2, c: 3};");
invalid("var {a, b, ...{d}} = {a: 1, b: 2, c: 3, d: 4};");
invalid("var {a, b, ...{d = 15}} = {a: 1, b: 2, c: 3, d: 4};");
invalid("var {a, b, ...{d = 15, ...r}} = {a: 1, b: 2, c: 3, d: 4};");
valid("(({a, b, ...r}) => {})({a: 1, b: 2, c: 3, d: 4});");
valid("(function ({a, b, ...r}) {})({a: 1, b: 2, c: 3, d: 4});");
valid("var a, b, c; ({a, b, ...r} = {a: 1, b: 2, c: 3, d: 4});");
valid("try { throw {a:2} } catch({...rest}) {}");
valid("let c = {}; let o = {a: 1, b: 2, ...c};");
valid("let o = {a: 1, b: 3, ...{}};");
valid("let o = {a: 1, b: 2, ...null, c: 3};");
valid("let o = {a: 1, b: 2, ...undefined, c: 3};");
valid("let o = {a: 1, b: 2, ...{...{}}, c: 3};");
valid("let c = {}; let o = {a: 1, b: 2, ...c, d: 3, ...c, e: 5};");
valid("let o = {a: 1, b: 2, ...d = {e: 2}, c: 3};");
valid("let p = true; let o = {a: 1, b: 2, ...d = p ? {e: 2} : {f: 4}, c: 3};");
valid("let o = {a: 1, b: 2, ...(a) => 3, c: 3};");
valid("function * foo() { return {a: 1, b: 2, ...yield, c: 3}; }");
invalid("function * foo(o) { ({...{ x = yield }} = o); }");
invalid("var {...r = {a: 2}} = {a: 1, b: 2};");
invalid("var {...r, b} = {a: 1, b: 2};");
invalid("var {...r, ...e} = {a: 1, b: 2};");
invalid("({...new Object()} = {a: 1, b: 2});");
invalid("(function * (o) { ({ ...{ x: yield } } = o); })()");
invalid("(function () {'use strict'; ({...eval} = {}); })()");
invalid("(function () {'use strict'; ({...arguments} = {}); })()");
invalid("async function foo () { let {...await} = {}; }");
invalid("let {...let} = {a: 1, b: 2};");
invalid("const {...let} = {a: 1, b: 2};");
invalid("try { throw {a:2} } catch({...foo.a}) {}");

debug("Rest parameter");
valid("function foo(...a) { }");
valid("function foo(a, ...b) { }");
valid("function foo(a = 20, ...b) { }");
valid("function foo(a, b, c, d, e, f, g, ...h) { }");
invalid("function foo(a, ...b, c) { }")
invalid("function foo(a, ...b, ) { }")
invalid("function foo(a, ...[b], ) { }")
invalid("function foo(a, ...{b}, ) { }")
invalid("function foo(a, ...a) { }");
invalid("function foo(...a, ...b) { }");
invalid("function foo(...b, ...b) { }");
invalid("function foo(...b  ...b) { }");
invalid("function foo(a, a, ...b) { }");
invalid("function foo(a, ...{b} = 20) { }");
invalid("function foo(a, ...b = 20) { }");
valid("function foo(...{b}) { }");
valid("function foo(...[b]) { }");
invalid("function foo(...123) { }");
invalid("function foo(...123abc) { }");
valid("function foo(...abc123) { }");
valid("function foo(...let) { }");
invalid("'use strict'; function foo(...let) { }");
invalid("'use strict'; function foo(...[let]) { }");
valid("function foo(...yield) { }");
invalid("'use strict'; function foo(...yield) { }");
invalid("function foo(...if) { }");
valid("let x = (...a) => { }");
valid("let x = (a, ...b) => { }");
valid("let x = (a = 20, ...b) => { }");
invalid("let x = (a = 20, ...b, ...c) => { }");
valid("let x = (a = 20, ...[...b]) => { }");
valid("let x = (a = 20, ...[...[b = 40]]) => { }");
valid("let x = (a = 20, ...{b}) => { }");
valid("let x = (a = 20, ...{...b}) => { }");
invalid("let x = (a = 20, ...{124}) => { }");

debug("non-simple parameter list")
invalid("function foo(...restParam) { 'use strict'; }");
invalid("function foo(...restParam) { 'a'; 'use strict'; }");
invalid("function foo({x}) { 'use strict'; }");
invalid("function foo({x}) { 'a'; 'use strict'; }");
invalid("function foo(a = 20) { 'use strict'; }");
invalid("function foo(a = 20) { 'a'; 'use strict'; }");
invalid("function foo({a} = 20) { 'use strict'; }");
invalid("function foo({a} = 20) { 'a'; 'use strict'; }");
invalid("function foo([a]) { 'use strict'; }");
invalid("function foo([a]) { 'a'; 'use strict'; }");
invalid("function foo(foo, bar, a = 25) { 'use strict'; }");
invalid("function foo(foo, bar, a = 25) { 'a'; 'use strict'; }");
invalid("function foo(foo, bar, baz, ...rest) { 'use strict'; }");
invalid("function foo(foo, bar, baz, ...rest) { 'a'; 'use strict'; }");
invalid("function foo(a = function() { }) { 'use strict'; a(); }");
invalid("function foo(a = function() { }) { 'a'; 'use strict'; a(); }");
invalid("let foo = (...restParam) => { 'use strict'; }");
invalid("let foo = (...restParam) => { 'a'; 'use strict'; }");
invalid("let foo = ({x}) => { 'use strict'; }");
invalid("let foo = ({x}) => { 'a'; 'use strict'; }");
invalid("let foo = (a = 20) => { 'use strict'; }");
invalid("let foo = (a = 20) => { 'a'; 'use strict'; }");
invalid("let foo = ({a} = 20) => { 'use strict'; }");
invalid("let foo = ({a} = 20) => { 'a'; 'use strict'; }");
invalid("let foo = ([a]) => { 'use strict'; }");
invalid("let foo = ([a]) => { 'a'; 'use strict'; }");
invalid("let foo = (foo, bar, a = 25) => { 'use strict'; }");
invalid("let foo = (foo, bar, a = 25) => { 'a'; 'use strict'; }");
invalid("let foo = (foo, bar, baz, ...rest) => { 'use strict'; }");
invalid("let foo = (foo, bar, baz, ...rest) => { 'a'; 'use strict'; }");
invalid("let foo = (a = function() { }) => { 'use strict'; a(); }");
invalid("let foo = (a = function() { }) => { 'a'; 'use strict'; a(); }");
valid("function outer() { 'use strict'; function foo(...restParam) {  } }");
valid("function outer() { 'use strict'; function foo(a,b,c,...restParam) {  } }");
valid("function outer() { 'use strict'; function foo(a = 20,b,c,...restParam) {  } }");
valid("function outer() { 'use strict'; function foo(a = 20,{b},c,...restParam) {  } }");
valid("function outer() { 'use strict'; function foo(a = 20,{b},[c] = 5,...restParam) {  } }");
valid("function outer() { 'use strict'; function foo(a = 20) {  } }");
valid("function outer() { 'use strict'; function foo(a,b,c,{d} = 20) {  } }");
invalid("function outer() { 'use strict'; function foo(...restParam) { 'use strict';  } }");
invalid("function outer() { 'use strict'; function foo(a,b,c,...restParam) {  'use strict'; } }");
invalid("function outer() { 'use strict'; function foo(a = 20,b,c,...restParam) {  'use strict'; } }");
invalid("function outer() { 'use strict'; function foo(a = 20,{b},c,...restParam) { 'use strict'; } }");
invalid("function outer() { 'use strict'; function foo(a = 20,{b},[c] = 5,...restParam) { 'use strict'; } }");
invalid("function outer() { 'use strict'; function foo(a = 20) {  'use strict';} }");
invalid("function outer() { 'use strict'; function foo(a,b,c,{d} = 20) { 'use strict'; } }");

debug("Arrow function");
valid("var x = (x) => x;");
valid("var x = (x, y, z) => x;");
valid("var x = ({x}, [y], z) => x;");
valid("var x = ({x = 30}, [y], z) => x;");
valid("var x = (x = 20) => x;");
valid("var x = ([x] = 20, y) => x;");
valid("var x = ([x = 20] = 20) => x;");
valid("var x = foo => x;");
valid("var x = foo => x => x => x => x;");
valid("var x = foo => x => (x = 20) => (x = 20) => x;");
valid("var x = foo => x => x => x => {x};");
valid("var x = ([x = 25]) => x => x => ({x} = {});");
invalid("var x = foo => x => x => {x} => x;");
invalid("var x = {x} = 20 => x;");
invalid("var x = [x] = 20 => x;");
invalid("var x = [x = 25] = 20 => x;");
invalid("var x = ([x = 25]) =>;");
invalid("var x = ([x = 25]) => x =>;");
invalid("var x = ([x = 25]) => x => x =>;");
invalid("var x = ([x = 25]) => x => x => {;");
invalid("var x ==> x;");
invalid("var x = x ==> x;");
valid("foo((x) => x)");
valid("foo((x, y, z) => x)");
valid("foo(({x}, [y], z) => x)");
valid("foo(({x = 30}, [y], z) => x)");
valid("foo((x = 20) => x)");
valid("foo(([x] = 20, y) => x)");
valid("foo(([x = 20] = 20) => x)");
valid("foo(foo => x)");
valid("foo(foo => x => x => x => x)");
valid("foo(foo => x => (x = 20) => (x = 20) => x)");
valid("foo(foo => x => x => x => {x})");
valid("foo(([x = 25]) => x => x => ({x} = {}))");
invalid("foo(foo => x => x => {x} => x)");
invalid("foo({x} = 20 => x)");
invalid("foo([x] = 20 => x)");
invalid("foo([x = 25] = 20 => x)");
invalid("foo(([x = 25]) =>)");
invalid("foo(([x = 25]) => x =>)");
invalid("foo(([x = 25]) => x => x =>)");
invalid("foo(([x = 25]) => x => x => {)");
invalid("foo(x ==> x)");
invalid("foo(x = x ==> x)");
valid("var f = cond ? ()=>20 : ()=>20");
valid("var f = cond ? (x)=>{x} : ()=>20");
valid("var f = cond ? (x)=>x : ()=>{2}");
valid("var f = cond ? (x)=>x : x=>2");
valid("var f = cond ? x=>x : x=>2");
valid("var f = cond ? x=>x*2 : x=>2");
valid("var f = cond ? x=>x.foo : x=>2");
valid("var f = cond ? x=>x.foo : x=>x + x + x + x + x + x + x");
valid("var f = cond ? x=>{x.foo } : x=>x + x + x + x + x + x + (x =>x) ");
valid("var f = (x) => x => (x) => ({y}) => y");
invalid("var f = cond ? x=>x.foo; : x=>x + x + x + x + x + x + x");
invalid("var f = cond ? x=>x.foo : : x=>x + x + x + x + x + x + x");
invalid("var f = cond ? x=>{x.foo :} : x=>x + x + x + x + x + x + x");
invalid("var f = cond ? x=>{x.foo } => : x=>x + x + x + x + x + x + x");
valid("class C { constructor() { this._x = 45; } get foo() { return this._x;} } class D extends C { x(y = () => super.foo) { return y(); } }");
valid("class C { constructor() { this._x = 45; } get foo() { return this._x;} } class D extends C { x(y = () => {return super.foo}) { return y(); } }");
valid("class C { constructor() { this._x = 45; } get foo() { return this._x;} } class D extends C { x(y = () => {return () => super.foo}) { return y()(); } }");
valid("class C { constructor() { this._x = 45; } get foo() { return this._x;} } class D extends C { x(y = (y = () => super.foo) => {return y()}) { return y(); } }");
valid("class C { constructor() { this._x = 45; } get foo() { return this._x;} } class D extends C { constructor(x = () => super.foo) { super(); this._x_f = x; } x() { return this._x_f(); } }");
valid("class C { constructor() { this._x = 45; } get foo() { return this._x;} } class D extends C { constructor(x = () => super()) { x(); } x() { return super.foo; } }");
invalid("let x = (a,a)=>a;");
invalid("let x = ([a],a)=>a;");
invalid("let x = ([a, a])=>a;");
invalid("let x = ({a, b:{a}})=>a;");
invalid("let x = (a,a)=>{ a };");
invalid("let x = ([a],a)=>{ };");
invalid("let x = ([a, a])=>{ };");
invalid("let x = (a, ...a)=>{ };");
invalid("let x = (b, c, b)=>{ };");
invalid("let x = (a, b, c, d, {a})=>{ };");
invalid("let x = (b = (a,a)=>a, b)=>{ };");
invalid("((a,a)=>a);");
invalid("let x = (a)\n=>a;");

invalid("({ foo(a,a){} });");
invalid("({ foo(b, c, b){} });");
invalid("({ *foo(a,a){} });");
invalid("({ *foo(b, c, b){} });");
invalid("({ async foo(a,a){} });");
invalid("({ async foo(b, c, b){} });");
valid("({ foo: function(a,a){} });");
valid("({ foo: function(b, c, b){} });");
valid("({ foo: function*(a,a){} });");
valid("({ foo: function*(b, c, b){} });");
valid("({ foo: async function(a,a){} });");
valid("({ foo: async function(b, c, b){} });");
invalid("({ foo({a},a){} });");
invalid("({ foo(a,{a}){} });");
invalid("({ foo(a,...a){} });");
invalid("({ foo({a},...a){} });");
invalid("({ foo({...a},...a){} });");
invalid("({ *foo({a},a){} });");
invalid("({ *foo(a,{a}){} });");
invalid("({ *foo(a,...a){} });");
invalid("({ *foo({a},...a){} });");
invalid("({ *foo({...a},...a){} });");
invalid("({ async foo({a},a){} });");
invalid("({ async foo(a,{a}){} });");
invalid("({ async foo(a,...a){} });");
invalid("({ async foo({a},...a){} });");
invalid("({ async foo({...a},...a){} });");
valid("({ foo(a, ...b){} });");
valid("({ foo({a}, ...b){} });");
valid("({ foo({a, ...b}){} });");
valid("({ foo({b, ...a}, ...c){} });");

debug("Weird things that used to crash.");
invalid(`or ([[{break //(elseifo (a=0;a<2;a++)n=
        [[{aFYY sga=
        [[{a=Yth FunctionRY&=Ylet 'a'}V a`)

try { eval("a.b.c = {};"); } catch(e1) { e=e1; shouldBe("e.line", "1") }
foo = 'FAIL';
bar = 'PASS';
try {
     eval("foo = 'PASS'; a.b.c = {}; bar  = 'FAIL';");
} catch(e) {
     shouldBe("foo", "'PASS'");
     shouldBe("bar", "'PASS'");
}
