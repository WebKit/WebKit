let section;
let failures = "";
let failureCount = 0;

function shouldBe(actual, expected) {
    if (typeof(actual) !== typeof(expected) || actual !== expected) {
        failures += ("   " + section + ": typeof expected: " + typeof(expected) + ", typeof actual: " + typeof(actual) + "\n");
        failures += ("       expected: '" + expected + "', actual: '" + actual + "'\n");
        failureCount++;
    }
}

section = "object method";
(function () {
    let foo = "good";
    let result = ({ foo () { return foo; } }).foo();
    shouldBe(result, "good");
})();
section = "object method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result = ({ [x] () { return foo; } }).foo();
    shouldBe(result, "good");
})();

section = "object getter method";
(function () {
    let foo = "good";
    let result;
    ({ get foo () { result = foo; } }).foo;
    shouldBe(result, "good");
})();
(function () {
    let get = "good";
    let result;
    ({ get foo () { result = get; } }).foo;
    shouldBe(result, "good");
})();
section = "object getter method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    ({ get [x] () { result = foo; } })[x];
    shouldBe(result, "good");
})();
(function () {
    let get = "good";
    let x = "get";
    let result;
    ({ get [x] () { result = get; } })[x];
    shouldBe(result, "good");
})();

section = "object setter method";
(function () {
    let foo = "good";
    let result;
    ({ set foo (v) { result = foo; } }).foo = 5;
    shouldBe(result, "good");
})();
(function () {
    let set = "good";
    let result;
    ({ set foo (v) { result = set; } }).foo = 5;
    shouldBe(result, "good");
})();
section = "object setter method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    ({ set [x] (v) { result = foo; } })[x] = 5;
    shouldBe(result, "good");
})();
(function () {
    let get = "good";
    let x = "get";
    let result;
    ({ set [x] (v) { result = get; } })[x] = 5;
    shouldBe(result, "good");
})();

section = "object generator method";
(function () {
    let foo = "good";
    let result;
    let value = ({ *foo () { result = foo; yield foo; } }).foo().next().value;
    shouldBe(result, "good");
    shouldBe(value, "good");
})();
section = "object generator method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    let value = ({ *[x] () { result = foo; yield foo; } })[x]().next().value;
    shouldBe(result, "good");
    shouldBe(value, "good");
})();

section = "class constructor";
(function () {
    let constructor = "good";
    let result;
    new class { constructor () { result = constructor; } };
    shouldBe(result, "good");
})();

section = "class static method";
(function () {
    let foo = "good";
    let result = (class { static foo () { return foo; } }).foo();
    shouldBe(result, "good");
})();
section = "class static method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    var result = (class { static [x] () { return foo; } })[x]();
    shouldBe(result, "good");
})();

section = "class instance method";
(function () {
    let foo = "good";
    let result = (new class { foo () { return foo; } }).foo();
    shouldBe(result, "good");
})();
section = "class instance method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result = (new class { [x] () { return foo; } })[x]();
    shouldBe(result, "good");
})();

section = "class static getter method";
(function () {
    let foo = "good";
    let result;
    (class { static get foo() { result = foo; } }).foo;
    shouldBe(result, "good");
})();
(function () {
    let get = "good";
    let result;
    (class { static get foo() { result = get; } }).foo;
    shouldBe(result, "good");
})();
section = "class static getter method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    (class { static get [x]() { result = foo; } })[x];
    shouldBe(result, "good");
})();
(function () {
    let get = "good";
    let x = "foo";
    let result;
    (class { static get [x]() { result = get; } })[x];
    shouldBe(result, "good");
})();

section = "class instance getter method";
(function () {
    let foo = "good";
    let result;
    (new class { get foo() { result = foo; } }).foo;
    shouldBe(result, "good");
})();
(function () {
    let get = "good";
    let result;
    (new class { get foo() { result = get; } }).foo;
    shouldBe(result, "good");
})();
section = "class instance getter method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    (new class { get [x]() { result = foo; } })[x];
    shouldBe(result, "good");
})();
(function () {
    let get = "good";
    let x = "foo";
    let result;
    (new class { get [x]() { result = get; } })[x];
    shouldBe(result, "good");
})();

section = "class static setter method";
(function () {
    let foo = "good";
    let result;
    (class { static set foo(v) { result = foo; } }).foo = 5;
    shouldBe(result, "good");
})();
(function () {
    let set = "good";
    let result;
    (class { static set foo(v) { result = set; } }).foo = 5;
    shouldBe(result, "good");
})();
section = "class static setter method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    (class { static set [x](v) { result = foo; } })[x] = 5;
    shouldBe(result, "good");
})();
(function () {
    let set = "good";
    let x = "foo";
    let result;
    (class { static set [x](v) { result = set; } })[x] = 5;
    shouldBe(result, "good");
})();

section = "class instance setter method";
(function () {
    let foo = "good";
    let result;
    (new class { set foo(v) { result = foo; } }).foo = 5;
    shouldBe(result, "good");
})();
(function () {
    let set = "good";
    let result;
    (new class { set foo(v) { result = set; } }).foo = 5;
    shouldBe(result, "good");
})();
section = "class instance setter  methodwith computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    (new class { set [x](v) { result = foo; } })[x] = 5;
    shouldBe(result, "good");
})();
(function () {
    let set = "good";
    let x = "foo";
    let result;
    (new class { set [x](v) { result = set; } })[x] = 5;
    shouldBe(result, "good");
})();

section = "class static generator method";
(function () {
    let foo = "good";
    let result;
    let value = (class { static *foo () { result = foo; yield foo; } }).foo().next().value;
    shouldBe(result, "good");
    shouldBe(value, "good");
})();
section = "class static generator method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    let value = (class { static *[x] () { result = foo; yield foo; } })[x]().next().value;
    shouldBe(result, "good");
    shouldBe(value, "good");
})();

section = "class instance generator method";
(function () {
    let foo = "good";
    let result;
    let value = (new class { *foo () { result = foo; yield foo; } }).foo().next().value;
    shouldBe(result, "good");
    shouldBe(value, "good");
})();
section = "class instance generator method with computed name";
(function () {
    let foo = "good";
    let x = "foo";
    let result;
    let value = (new class { *[x] () { result = foo; yield foo; } })[x]().next().value;
    shouldBe(result, "good");
    shouldBe(value, "good");
})();

if (failureCount)
    throw Error("Found " + failureCount + " failures:\n" + failures);
