description('Tests for ES6 class syntax containing semicolon in the class body');

shouldThrow("class A { foo;() { } }", "'SyntaxError: Unexpected token \\';\\'. Expected an opening \\'(\\' before a method\\'s parameter list.'");
shouldThrow("class A { foo() ; { } }", "'SyntaxError: Unexpected token \\\';\\'. Expected an opening \\'{\\' at the start of a method body.'");
shouldThrow("class A { get ; foo() { } }", "'SyntaxError: Unexpected token \\';\\'. Expected an opening \\'(\\' before a method\\'s parameter list.'");
shouldThrow("class A { get foo;() { } }", "'SyntaxError: Unexpected token \\\';\\'. Expected a parameter list for getter definition.'");
shouldThrow("class A { get foo() ; { } }", "'SyntaxError: Unexpected token \\\';\\'. Expected an opening \\'{\\' at the start of a getter body.'");
shouldThrow("class A { set ; foo(x) { } }", "'SyntaxError: Unexpected token \\';\\'. Expected an opening \\'(\\' before a method\\'s parameter list.'");
shouldThrow("class A { set foo;(x) { } }", "'SyntaxError: Unexpected token \\\';\\'. Expected a parameter list for setter definition.'");
shouldThrow("class A { set foo(x) ; { } }", "'SyntaxError: Unexpected token \\\';\\'. Expected an opening \\'{\\' at the start of a setter body.'");

shouldNotThrow("class A { ; }");
shouldNotThrow("class A { foo() { } ; }");
shouldNotThrow("class A { get foo() { } ; }");
shouldNotThrow("class A { set foo(x) { } ; }");
shouldNotThrow("class A { static foo() { } ; }");
shouldNotThrow("class A { static get foo() { } ; }");
shouldNotThrow("class A { static set foo(x) { } ; }");

shouldNotThrow("class A { ; foo() { } }");
shouldNotThrow("class A { ; get foo() { } }");
shouldNotThrow("class A { ; set foo(x) { } }");
shouldNotThrow("class A { ; static foo() { } }");
shouldNotThrow("class A { ; static get foo() { } }");
shouldNotThrow("class A { ; static set foo(x) { } }");

shouldNotThrow("class A { foo() { } ; foo() {} }");
shouldNotThrow("class A { foo() { } ; get foo() {} }");
shouldNotThrow("class A { foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { foo() { } ; static foo() {} }");
shouldNotThrow("class A { foo() { } ; static get foo() {} }");
shouldNotThrow("class A { foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { get foo() { } ; foo() {} }");
shouldNotThrow("class A { get foo() { } ; get foo() {} }");
shouldNotThrow("class A { get foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { get foo() { } ; static foo() {} }");
shouldNotThrow("class A { get foo() { } ; static get foo() {} }");
shouldNotThrow("class A { get foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { set foo(x) { } ; foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; get foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; set foo(x) {} }");
shouldNotThrow("class A { set foo(x) { } ; static foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; static get foo() {} }");
shouldNotThrow("class A { set foo(x) { } ; static set foo(x) {} }");

shouldNotThrow("class A { static foo() { } ; foo() {} }");
shouldNotThrow("class A { static foo() { } ; get foo() {} }");
shouldNotThrow("class A { static foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { static foo() { } ; static foo() {} }");
shouldNotThrow("class A { static foo() { } ; static get foo() {} }");
shouldNotThrow("class A { static foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { static get foo() { } ; foo() {} }");
shouldNotThrow("class A { static get foo() { } ; get foo() {} }");
shouldNotThrow("class A { static get foo() { } ; set foo(x) {} }");
shouldNotThrow("class A { static get foo() { } ; static foo() {} }");
shouldNotThrow("class A { static get foo() { } ; static get foo() {} }");
shouldNotThrow("class A { static get foo() { } ; static set foo(x) {} }");

shouldNotThrow("class A { static set foo(x) { } ; foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; get foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; set foo(x) {} }");
shouldNotThrow("class A { static set foo(x) { } ; static foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; static get foo() {} }");
shouldNotThrow("class A { static set foo(x) { } ; static set foo(x) {} }");

var successfullyParsed = true;
