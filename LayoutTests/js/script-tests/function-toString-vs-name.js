// isWhiteSpace() and stripSpaces() are borrowed from
// kde/script-tests/inbuilt_function_tostring.js and modified.

let section;
let failures = "";
let failureCount = 0;

let nonSymbolValues = [ "foo", "", undefined, null, true, false, 0, 10, 1234.567 ];
let symbolValues = [
    { v: Symbol("foo"), name: "[foo]" },
    { v: Symbol(""), name: "[]" },
    { v: Symbol(), name: "" },
];

function isWhiteSpace(string) {
    let cc = string.charCodeAt(0);
    switch (cc) {
        case (0x0009):
        case (0x000B):
        case (0x000C):
        case (0x0020):
        case (0x000A):
        case (0x000D):
        case (59): // let's strip out semicolons, too
            return true;
            break;
        default:
            return false;
    }
}

function minimizeSpaces(s) {
    let currentChar;
    let strippedString;
    let previousCharIsWhiteSpace = false;
    for (currentChar = 0, strippedString = ""; currentChar < s.length; currentChar++) {
        let ch = s.charAt(currentChar);
        if (!isWhiteSpace(ch)) {
            if (previousCharIsWhiteSpace &&
                (ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '[' || ch == ']'))
                strippedString = strippedString.slice(0, strippedString.length - 1);
            strippedString += ch;
            previousCharIsWhiteSpace = false;
        } else if (!previousCharIsWhiteSpace) {
            strippedString += ' ';
            previousCharIsWhiteSpace = true;
        }
    }
    return strippedString;
}

function shouldBe(desc, funcName, actual, expected) {
    if (typeof(actual) !== typeof(expected) || actual !== expected) {
        failures += ("   " + section + ": " + desc + "'" + funcName + "': typeof expected: " + typeof(expected) + ", typeof actual: " + typeof(actual) + "\n");
        failures += ("       expected: '" + expected + "', actual: '" + actual + "'\n");
        failureCount++;
    }
}

function toString(x) {
    if (typeof x === "symbol")
        return x.toString();
    return "" + x;
}

function test(func, expectedName, expectedToString) {
    shouldBe("Function.name on ", expectedName, func.name, expectedName);

    let str = func.toString();
    shouldBe("Function#toString on ", expectedName, minimizeSpaces(str), minimizeSpaces(expectedToString));

    let origDesc = Object.getOwnPropertyDescriptor(func, "name");
    shouldBe("Function.name configurability of ", expectedName, origDesc.configurable, true);
    shouldBe("Function.name writability of ", expectedName, origDesc.writable, false);
    shouldBe("Function.name enumerability of ", expectedName, origDesc.enumerable, false);

    // We should not be able to change Function.name while it is not writable.
    let origFuncName = func.name;
    let modifiedFuncName = "modified_" + toString(origFuncName);
    func.name = modifiedFuncName;
    shouldBe("Function.name (after attempted write) on ", expectedName, func.name, expectedName);

    // We should be able to change Function.name after making it writable.
    Object.defineProperty(func, "name", { writable: true });
    let modifiedDesc = Object.getOwnPropertyDescriptor(func, "name");
    shouldBe("Function.name writability (after made writable) of ", expectedName, modifiedDesc.writable, true);

    func.name = modifiedFuncName;
    shouldBe("Function.name (after attempted write again) on ", expectedName, func.name, modifiedFuncName);

    // But the toString name should not have changed.
    str = func.toString();
    shouldBe("Function#toString (after attempted write) on ", expectedName, minimizeSpaces(str), minimizeSpaces(expectedToString));

    // Put things back to the original state.
    Object.defineProperty(func, "name", origDesc);
}

section = "builtin function";
{
    test(Array.prototype.every, "every", "function every() { [native code] }");
    test(Array.prototype.forEach, "forEach", "function forEach() { [native code] }");
    test(Array.prototype.some, "some", "function some() { [native code] }");

    section = "bound builtin function";
    {
        let o = {}
        let boundEvery = Array.prototype.every.bind(o);
        test(boundEvery, "bound every", "function every() { [native code] }");
        let boundForEach = Array.prototype.forEach.bind(o);
        test(boundForEach, "bound forEach", "function forEach() { [native code] }");
    }
}

section = "native function";
{
    test(Array.prototype.splice, "splice", "function splice() { [native code] }");
    test(Array.prototype.unshift, "unshift", "function unshift() { [native code] }");
    test(Array.prototype.indexOf, "indexOf", "function indexOf() { [native code] }");

    section = "bound native function";
    {
        let o = {}
        let boundSplice = Array.prototype.splice.bind(o);
        test(boundSplice, "bound splice", "function splice() { [native code] }");
        let boundUnshift = Array.prototype.unshift.bind(o);
        test(boundUnshift, "bound unshift", "function unshift() { [native code] }");
    }
}

section = "InternalFunction";
{
    test(Array, "Array", "function Array() { [native code] }");
    test(Boolean, "Boolean", "function Boolean() { [native code] }");

    section = "bound InternalFunction";
    {
        let o = {}
        let boundArray = Array.bind(o);
        test(boundArray, "bound Array", "function Array() { [native code] }");
        let boundBoolean = Boolean.bind(o);
        test(boundBoolean, "bound Boolean", "function Boolean() { [native code] }");
    }
}

section = "JS function";
{
    function foo1() {}
    test(foo1, "foo1", "function foo1() {}");

    let foo2 = function() {}
    test(foo2, "foo2", "function() {}");

    let foo3 = (function() {
        function goo3() {}
        return goo3;
    }) ();
    test(foo3, "goo3", "function goo3() {}");

    let foo4 = (function() {
        return (function() {});
    }) ();
    test(foo4, "", "function() {}");

    section = "bound JS function";
    {
        let o = {}
        let boundFoo1 = foo1.bind(o);
        test(boundFoo1, "bound foo1", "function foo1() { [native code] }");
        let boundFoo2 = foo2.bind(o);
        test(boundFoo2, "bound foo2", "function foo2() { [native code] }");
        let boundFoo3 = foo3.bind(o);
        test(boundFoo3, "bound goo3", "function goo3() { [native code] }");
        let boundFoo4 = foo4.bind(o);
        test(boundFoo4, "bound ", "function () { [native code] }");

        test((function(){}).bind({}), "bound ", "function () { [native code] }");
    }
}

section = "Object property";
{
    let o = {
        prop1: function() {},
        prop2: function namedProp2() {}
    };
    o.prop3 = function() { };
    o.prop4 = function namedProp4() {};
    test(o.prop1, "prop1", "function() {}");
    test(o.prop2, "namedProp2", "function namedProp2() {}");
    test(o.prop3, "", "function() {}");
    test(o.prop4, "namedProp4", "function namedProp4() {}");

    section = "bound Object property";
    {
        let boundProp1 = o.prop1.bind({});
        test(boundProp1, "bound prop1", "function prop1() { [native code] }");
        let boundFoo2 = o.prop2.bind({});
        test(boundFoo2, "bound namedProp2", "function namedProp2() { [native code] }");
        let boundFoo3 = o.prop3.bind({});
        test(boundFoo3, "bound ", "function() { [native code] }");
        let boundFoo4 = o.prop4.bind({});
        test(boundFoo4, "bound namedProp4", "function namedProp4() { [native code] }");
    }
}

section = "object shorthand method";
{
    let o = {
        prop1() {},
        prop2(x) {}
    };
    test(o.prop1, "prop1", "function prop1() {}");
    test(o.prop2, "prop2", "function prop2(x) {}");

    section = "bound object shorthand method";
    {
        let boundProp1 = o.prop1.bind(o);
        test(boundProp1, "bound prop1", "function prop1() { [native code] }");
        let boundProp2 = o.prop2.bind(o);
        test(boundProp2, "bound prop2", "function prop2() { [native code] }");
    }
}

section = "class from statement";
(function () {
    class foo {}
    class bar {}
    class bax { static name() {} }
    let baz = bar;
    class goo extends foo {}

    test(foo, "foo", "class foo {}");
    test(bar, "bar", "class bar {}");
    shouldBe("typeof bax.name of ", "bax", typeof bax.name, "function");
    shouldBe("toString of ", "bax", bax.toString(), "class bax { static name() {} }");
    test(baz, "bar", "class bar {}");
    test(goo, "goo", "class goo extends foo {}");

    section = "bound class from statement";
    {
        let bound1 = foo.bind({});
        test(bound1, "bound foo", "function foo() { [native code] }");
        let bound2 = bar.bind({});
        test(bound2, "bound bar", "function bar() { [native code] }");
        let bound3 = bax.bind({});
        test(bound3, "bound ", "function() { [native code] }"); // bax.name is not a string.
        let bound4 = baz.bind({});
        test(bound4, "bound bar", "function bar() { [native code] }");
        let bound5 = goo.bind({});
        test(bound5, "bound goo", "function goo() { [native code] }");
    }
})();

section = "class with constructor from statement";
(function () {
    class foo { constructor(x) {} }
    class bar { constructor() {} }
    class bax { static name() {} constructor() {} }
    let baz = bar;
    class goo extends foo { constructor() { super(5); } }

    test(foo, "foo", "class foo { constructor(x) {} }");
    test(bar, "bar", "class bar { constructor() {} }");
    shouldBe("typeof bax.name of ", "bax", typeof bax.name, "function");
    shouldBe("toString of ", "bax", bax.toString(), "class bax { static name() {} constructor() {} }");
    test(baz, "bar", "class bar { constructor() {} }");
    test(goo, "goo", "class goo extends foo { constructor() { super(5); } }");

    section = "bound class with constructor from statement";
    {
        let bound1 = foo.bind({});
        test(bound1, "bound foo", "function foo() { [native code] }");
        let bound2 = bar.bind({});
        test(bound2, "bound bar", "function bar() { [native code] }");
        let bound3 = bax.bind({});
        test(bound3, "bound ", "function() { [native code] }"); // bax.name is not a string.
        let bound4 = baz.bind({});
        test(bound4, "bound bar", "function bar() { [native code] }");
        let bound5 = goo.bind({});
        test(bound5, "bound goo", "function goo() { [native code] }");
    }
})();

section = "class from expression";
(function () {
    let foo = class namedFoo {}
    let bar = class {}
    let bax = class { static name() {} }
    let baz = bar;
    let goo = class extends foo {}

    test(foo, "namedFoo", "class namedFoo {}");
    test(bar, "bar", "class {}");
    shouldBe("typeof bax.name of ", "bax", typeof bax.name, "function");
    shouldBe("toString of ", "bax", bax.toString(), "class { static name() {} }");
    test(baz, "bar", "class {}");
    test(goo, "goo", "class extends foo {}");

    section = "bound class from expression";
    {
        let bound1 = foo.bind({});
        test(bound1, "bound namedFoo", "function namedFoo() { [native code] }");
        let bound2 = bar.bind({});
        test(bound2, "bound bar", "function bar() { [native code] }");
        let bound3 = bax.bind({});
        test(bound3, "bound ", "function() { [native code] }"); // bax.name is not a string.
        let bound4 = baz.bind({});
        test(bound4, "bound bar", "function bar() { [native code] }");
        let bound5 = goo.bind({});
        test(bound5, "bound goo", "function goo() { [native code] }");
    }
})();

section = "class with constructor from expression";
(function () {
    let foo = class namedFoo { constructor(x) {} }
    let bar = class { constructor() {} }
    let bax = class { static name() {} constructor() {} }
    let baz = bar;
    let goo = class extends foo { constructor() { super(x) } }

    test(foo, "namedFoo", "class namedFoo { constructor(x) {} }");
    test(bar, "bar", "class { constructor() {} }");
    shouldBe("typeof bax.name of ", "bax", typeof bax.name, "function");
    shouldBe("toString of ", "bax", bax.toString(), "class { static name() {} constructor() {} }");
    test(baz, "bar", "class { constructor() {} }");
    test(goo, "goo", "class extends foo { constructor() { super(x) } }");

    section = "bound class with constructor from expression";
    {
        let bound1 = foo.bind({});
        test(bound1, "bound namedFoo", "function namedFoo() { [native code] }");
        let bound2 = bar.bind({});
        test(bound2, "bound bar", "function bar() { [native code] }");
        let bound3 = bax.bind({});
        test(bound3, "bound ", "function() { [native code] }"); // bax.name is not a string.
        let bound4 = baz.bind({});
        test(bound4, "bound bar", "function bar() { [native code] }");
        let bound5 = goo.bind({});
        test(bound5, "bound goo", "function goo() { [native code] }");
    }
})();

section = "class in object property";
(function () {
    class gooBase {}
    let o = {
        foo: class {},
        bar: class {},
        bax: class { static name() {} },
        goo: class extends gooBase {},
    };
    o.bay = o.bar;
    o.baz = class {};

    test(o.foo, "foo", "class {}");
    test(o.bar, "bar", "class {}");
    shouldBe("typeof o.bax.name of ", "o.bax", typeof o.bax.name, "function");
    shouldBe("toString of ", "o.bax", o.bax.toString(), "class { static name() {} }");
    test(o.bay, "bar", "class {}");
    test(o.baz, "", "class {}");
    test(o.goo, "goo", "class extends gooBase {}");

    section = "bound class in object property";
    {
        let bound1 = o.foo.bind({});
        test(bound1, "bound foo", "function foo() { [native code] }");
        let bound2 = o.bar.bind({});
        test(bound2, "bound bar", "function bar() { [native code] }");
        let bound3 = o.bax.bind({});
        test(bound3, "bound ", "function() { [native code] }"); // bax.name is not a string.
        let bound4 = o.bay.bind({});
        test(bound4, "bound bar", "function bar() { [native code] }");
        let bound5 = o.baz.bind({});
        test(bound5, "bound ", "function() { [native code] }");
        let bound6 = o.goo.bind({});
        test(bound6, "bound goo", "function goo() { [native code] }");
    }
})();

section = "class with constructor in object property";
(function () {
    class gooBase { constructor(x) {} }
    let o = {
        foo: class { constructor(x) {} },
        bar: class { constructor() {} },
        bax: class { static name() {} constructor() {} },
        goo: class extends gooBase { constructor() { super(5); } },
    };
    o.bay = o.bar;
    o.baz = class { constructor() {} };

    test(o.foo, "foo", "class { constructor(x) {} }");
    test(o.bar, "bar", "class { constructor() {} }");
    shouldBe("typeof o.bax.name of ", "o.bax", typeof o.bax.name, "function");
    shouldBe("toString of ", "o.bax", o.bax.toString(), "class { static name() {} constructor() {} }");
    test(o.bay, "bar", "class { constructor() {} }");
    test(o.baz, "", "class { constructor() {} }");
    test(o.goo, "goo", "class extends gooBase { constructor() { super(5); } }");

    section = "bound class with constructor in object property";
    {
        let bound1 = o.foo.bind({});
        test(bound1, "bound foo", "function foo() { [native code] }");
        let bound2 = o.bar.bind({});
        test(bound2, "bound bar", "function bar() { [native code] }");
        let bound3 = o.bax.bind({});
        test(bound3, "bound ", "function() { [native code] }"); // bax.name is not a string.
        let bound4 = o.bay.bind({});
        test(bound4, "bound bar", "function bar() { [native code] }");
        let bound5 = o.baz.bind({});
        test(bound5, "bound ", "function() { [native code] }");
        let bound6 = o.goo.bind({});
        test(bound6, "bound goo", "function goo() { [native code] }");
    }
})();

section = "global class statement";
// Checking if there are CodeCache badness that can result from global class statements
// with identical bodies.
class globalCS1 { constructor(x) { return x; } stuff() { return 5; } }
// Identical class body as CS1.
class globalCS2 { constructor(x) { return x; } stuff() { return 5; } }
// Identical constructor as CS2 & CS1, but different otherwise.
class globalCS3 { constructor(x) { return x; } stuff3() { return 15; } }

test(globalCS1, "globalCS1", "class globalCS1 { constructor(x) { return x; } stuff() { return 5; } }");
test(globalCS2, "globalCS2", "class globalCS2 { constructor(x) { return x; } stuff() { return 5; } }");
test(globalCS3, "globalCS3", "class globalCS3 { constructor(x) { return x; } stuff3() { return 15; } }");

section = "global class expression";
// Checking if there are CodeCache badness that can result from global class expressions
// with identical bodies.
var globalCE1 = class { constructor(x) { return x; } stuff() { return 5; } }
// Identical class body as CSE1.
var globalCE2 = class { constructor(x) { return x; } stuff() { return 5; } }
// Identical constructor as CSE2 & CSE1, but different otherwise.
var globalCE3 = class { constructor(x) { return x; } stuff3() { return 15; } }

test(globalCE1, "globalCE1", "class { constructor(x) { return x; } stuff() { return 5; } }");
test(globalCE2, "globalCE2", "class { constructor(x) { return x; } stuff() { return 5; } }");
test(globalCE3, "globalCE3", "class { constructor(x) { return x; } stuff3() { return 15; } }");

section = "class statements in eval";
// Checking if there are CodeCache badness that can result from class statements in
// identical eval statements.
(function () {
    let body1 = "class foo { constructor(x) { return x; } stuff() { return 5; } }; foo";
    // Identical class body as body1.
    let body2 = "class foo { constructor(x) { return x; } stuff() { return 5; } }; foo";
    // Identical constructor as body1 & body2, but different otherwise.
    let body3 = "class foo3 { constructor(x) { return x; } stuff3() { return 15; } }; foo3";

    let bar1 = eval(body1);
    let bar2 = eval(body2);
    let bar3 = eval(body3);
    let bar4 = eval(body1);

    test(bar1, "foo", "class foo { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar2, "foo", "class foo { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar3, "foo3", "class foo3 { constructor(x) { return x; } stuff3() { return 15; } }");
    test(bar4, "foo", "class foo { constructor(x) { return x; } stuff() { return 5; } }");
})();

section = "class expressions in eval";
// Checking if there are CodeCache badness that can result from class expressions in
// identical eval statements.
(function () {
    let body1 = "var foo = class { constructor(x) { return x; } stuff() { return 5; } }; foo";
    // Identical class body as body1.
    let body2 = "var foo = class { constructor(x) { return x; } stuff() { return 5; } }; foo";
    // Identical constructor as body1 & body2, but different otherwise.
    let body3 = "var foo3 = class { constructor(x) { return x; } stuff3() { return 15; } }; foo3";

    let bar1 = eval(body1);
    let bar2 = eval(body2);
    let bar3 = eval(body3);
    let bar4 = eval(body1);

    test(bar1, "foo", "class { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar2, "foo", "class { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar3, "foo3", "class { constructor(x) { return x; } stuff3() { return 15; } }");
    test(bar4, "foo", "class { constructor(x) { return x; } stuff() { return 5; } }");
})();

section = "class statements in dynamically created Functions";
// Checking if there are CodeCache badness that can result from dynamically created
// Function objects with class statements in identical bodies.
(function () {
    let body1 = "class foo { constructor(x) { return x; } stuff() { return 5; } } return foo;";
    // Identical class body as body1.
    let body2 = "class foo { constructor(x) { return x; } stuff() { return 5; } } return foo;";
    // Identical constructor as body1 & body2, but different otherwise.
    let body3 = "class foo3 { constructor(x) { return x; } stuff3() { return 15; } } return foo3;";

    let bar1 = new Function(body1);
    let bar2 = new Function(body2);
    let bar3 = new Function(body3);

    test(bar1(), "foo", "class foo { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar2(), "foo", "class foo { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar3(), "foo3", "class foo3 { constructor(x) { return x; } stuff3() { return 15; } }");
})();

section = "class expressions in dynamically created Functions";
// Checking if there are CodeCache badness that can result from dynamically created
// Function objects with class expressions in identical bodies.
(function () {
    let body1 = "var foo = class { constructor(x) { return x; } stuff() { return 5; } }; return foo;";
    // Identical class body as body1.
    let body2 = "var foo = class { constructor(x) { return x; } stuff() { return 5; } }; return foo;";
    // Identical constructor as body1 & body2, but different otherwise.
    let body3 = "var foo3 = class { constructor(x) { return x; } stuff3() { return 15; } }; return foo3;";

    let bar1 = new Function(body1);
    let bar2 = new Function(body2);
    let bar3 = new Function(body3);

    test(bar1(), "foo", "class { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar2(), "foo", "class { constructor(x) { return x; } stuff() { return 5; } }");
    test(bar3(), "foo3", "class { constructor(x) { return x; } stuff3() { return 15; } }");
})();

section = "Object computed non-symbol anonymous function property";
(function() {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            [value]: function() {},
        }
        test(o[value], toString(value), "function() {}");

        let bound = o[value].bind({});
        test(bound, "bound " + toString(value), "function " + toString(value) + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed non-symbol named function property";
(function() {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            [value]: function bar() {},
        }
        test(o[value], "bar", "function bar() {}");

        let bound = o[value].bind({});
        test(bound, "bound bar", "function bar() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed non-symbol property with shorthand function";
(function () {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            [value]() {},
        }
        test(o[value], toString(value), "function() {}");

        let bound = o[value].bind({});
        test(bound, "bound " + toString(value), "function " + toString(value) + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed non-symbol property with get/set function";
(function () {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            get [value]() {},
            set [value](x) {},
        }

        let desc = Object.getOwnPropertyDescriptor(o, value);
        test(desc.get, "get " + value, "function() {}");
        test(desc.set, "set " + value, "function(x) {}");

        let bound = desc.get.bind({});
        test(bound, "bound get " + toString(value), "function get " + toString(value) + "() { [native code] }");
        bound = desc.set.bind({});
        test(bound, "bound set " + toString(value), "function set " + toString(value) + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed symbol anonymous function property";
(function() {
    let values = symbolValues;
    function runTest(value, expectedName) {
        let o = {    
            [value]: function() {},
        }
        test(o[value], expectedName, "function() {}");

        let bound = o[value].bind({});
        test(bound, "bound " + expectedName, "function " + expectedName + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v, values[i].name);
})();

section = "Object computed symbol named function property";
(function() {
    let values = symbolValues;
    function runTest(value) {
        let o = {    
            [value]: function bar() {},
        }
        test(o[value], "bar", "function bar() {}");

        let bound = o[value].bind({});
        test(bound, "bound bar", "function bar() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v);
})();

section = "Object computed symbol property with shorthand function";
(function() {
    let values = symbolValues;
    function runTest(value, expectedName) {
        let o = {    
            [value]() {},
        }
        test(o[value], expectedName, "function() {}");

        let bound = o[value].bind({});
        test(bound, "bound " + expectedName, "function " + expectedName + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v, values[i].name);
})();

section = "Object computed symbol property with get/set function";
(function () {
    let values = symbolValues;
    function runTest(value, expectedName) {
        let o = {    
            get [value]() {},
            set [value](x) {},
        }

        let desc = Object.getOwnPropertyDescriptor(o, value);
        test(desc.get, "get " + expectedName, "function() {}");
        test(desc.set, "set " + expectedName, "function(x) {}");

        let bound = desc.get.bind({});
        test(bound, "bound get " + expectedName, "function get " + expectedName + "() { [native code] }");
        bound = desc.set.bind({});
        test(bound, "bound set " + expectedName, "function set " + expectedName + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v, values[i].name);
})();

// Test functions in destructuring assignments.
section = "destructuring assignment";
{
    let prop1;
    let prop2;
    { prop1 = function() {}, prop2 = function namedProp2() {} }
    test(prop1, "prop1", "function() {}");
    test(prop2, "namedProp2", "function namedProp2() {}");

    section = "bound destructuring assignment";
    {
        let o = {}
        let bound1 = prop1.bind(o);
        test(bound1, "bound prop1", "function prop1() { [native code] }");

        let bound2 = prop2.bind(o);
        test(bound2, "bound namedProp2", "function namedProp2() { [native code] }");
    }
}

section = "dynamically created function";
{
    let dynamic1 = new Function("");
    test(dynamic1, "anonymous", "function anonymous() {}");

    let dynamic2 = new Function("");
    dynamic2.name = "Goo2";
    test(dynamic2, "anonymous", "function anonymous() {}");

    section = "bound dynamically created function";
    {
        let o = {}
        let bound1 = dynamic1.bind(o);
        test(bound1, "bound anonymous", "function anonymous() { [native code] }");

        let bound2 = dynamic2.bind(o);
        test(bound2, "bound anonymous", "function anonymous() { [native code] }");
    }
}

section = "JSBoundSlotBaseFunction";
{
    if (typeof document !== "undefined") {
        let desc = Object.getOwnPropertyDescriptor(document, "location");
        test(desc.get, "get location", "function location() { [native code] }");
        test(desc.set, "set location", "function location() { [native code] }");

        section = "bound JSBoundSlotBaseFunction";
        {
            let o = {}
            let bound1 = desc.get.bind(o);
            test(bound1, "bound get location", "function get location() { [native code] }");

            let bound2 = desc.set.bind(o);
            test(bound2, "bound set location", "function set location() { [native code] }");
        }
    }
}

section = "get/set function";
{
    let o = { get foo() {}, set foo(x){} };
    let desc = Object.getOwnPropertyDescriptor(o, "foo");
    test(desc.get, "get foo", "function() {}");
    test(desc.set, "set foo", "function(x) {}");

    let o1 = { get "bar"() {}, set "bar"(x){} };
    let desc1 = Object.getOwnPropertyDescriptor(o1, "bar");
    test(desc1.get, "get bar", "function() {}");
    test(desc1.set, "set bar", "function(x) {}");

    let o2 = { get 100() {}, set 100(x){} };
    let desc2 = Object.getOwnPropertyDescriptor(o2, 100);
    test(desc2.get, "get 100", "function() {}");
    test(desc2.set, "set 100", "function(x) {}");

    let o3 = { get [100]() {}, set [100](x){} };
    let desc3 = Object.getOwnPropertyDescriptor(o3, 100);
    test(desc3.get, "get 100", "function() {}");
    test(desc3.set, "set 100", "function(x) {}");

    section = "bound get/set function";
    {
        let bound1;
        let bound2;

        bound1 = desc.get.bind(o);
        test(bound1, "bound get foo", "function get foo() { [native code] }");
        bound2 = desc.set.bind(o);
        test(bound2, "bound set foo", "function set foo() { [native code] }");

        bound1 = desc1.get.bind(o);
        test(bound1, "bound get bar", "function get bar() { [native code] }");
        bound2 = desc1.set.bind(o);
        test(bound2, "bound set bar", "function set bar() { [native code] }");

        bound1 = desc2.get.bind(o);
        test(bound1, "bound get 100", "function get 100() { [native code] }");
        bound2 = desc2.set.bind(o);
        test(bound2, "bound set 100", "function set 100() { [native code] }");

        bound1 = desc3.get.bind(o);
        test(bound1, "bound get 100", "function get 100() { [native code] }");
        bound2 = desc3.set.bind(o);
        test(bound2, "bound set 100", "function set 100() { [native code] }");
    }
}

// https://tc39.github.io/ecma262/#sec-function.prototype.bind
//     9. Let targetName be ? Get(Target, "name").
//    10. If Type(targetName) is not String, let targetName be the empty string.
//    11. Perform SetFunctionName(F, targetName, "bound").
section = "bound functions with non-string names";
{
    let foo = function() {};
    let bound;

    function setName(func, newName) {
        Object.defineProperty(func, "name", { writable:true });
        func.name = newName;
        Object.defineProperty(func, "name", { writable:false });
    }

    setName(foo, undefined);
    test(foo, undefined, "function() {}");
    bound = foo.bind({});
    test(bound, "bound ", "function () { [native code] }");    

    setName(foo, null);
    test(foo, null, "function() {}");
    bound = foo.bind({});
    test(bound, "bound ", "function () { [native code] }");    

    setName(foo, true);
    test(foo, true, "function() {}");
    bound = foo.bind({});
    test(bound, "bound ", "function () { [native code] }");    

    setName(foo, false);
    test(foo, false, "function() {}");
    bound = foo.bind({});
    test(bound, "bound ", "function () { [native code] }");    

    setName(foo, 1234.567);
    test(foo, 1234.567, "function() {}");
    bound = foo.bind({});
    test(bound, "bound ", "function () { [native code] }");    

    let anonSym = Symbol();
    setName(foo, anonSym);
    test(foo, anonSym, "function() {}");
    bound = foo.bind({});
    test(bound, "bound ", "function () { [native code] }");    

    let sym = Symbol("baz");
    setName(foo, sym);
    test(foo, sym, "function() {}");
    bound = foo.bind({});
    test(bound, "bound ", "function () { [native code] }");    
}

section = "Object computed non-symbol anonymous class property";
(function() {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            [value]: class {},
        }
        test(o[value], toString(value), "class {}");

        let bound = o[value].bind({});
        test(bound, "bound " + toString(value), "function " + toString(value) + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed non-symbol named class property";
(function() {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            [value]: class bar {},
        }
        test(o[value], "bar", "class bar {}");

        let bound = o[value].bind({});
        test(bound, "bound bar", "function bar() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed symbol anonymous class property";
(function() {
    let values = symbolValues;
    function runTest(value, expectedName) {
        let o = {    
            [value]: class {},
        }
        test(o[value], expectedName, "class {}");

        let bound = o[value].bind({});
        test(bound, "bound " + expectedName, "function " + expectedName + "() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v, values[i].name);
})();

section = "Object computed symbol named class property";
(function() {
    let values = symbolValues;
    function runTest(value) {
        let o = {    
            [value]: class bar {},
        }
        test(o[value], "bar", "class bar {}");

        let bound = o[value].bind({});
        test(bound, "bound bar", "function bar() { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v);
})();

section = "Object computed non-symbol anonymous class (with name method) property";
(function() {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            [value]: class { static name(){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class { static name(){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed non-symbol named class (with name method) property";
(function() {
    let values = nonSymbolValues;
    function runTest(value) {
        let o = {    
            [value]: class bar { static name(){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class bar { static name(){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed symbol anonymous class (with name method) property";
(function() {
    let values = symbolValues;
    function runTest(value, expectedName) {
        let o = {    
            [value]: class { static name(){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class { static name(){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v, values[i].name);
})();

section = "Object computed symbol named class (with name method) property";
(function() {
    let values = symbolValues;
    function runTest(value) {
        let o = {    
            [value]: class bar { static name(){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class bar { static name(){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v);
})();

section = "Object computed non-symbol anonymous class (with computed name method) property";
(function() {
    let values = nonSymbolValues;
    let nameStr = "name";
    function runTest(value) {
        let o = {    
            [value]: class { static [nameStr](){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class { static [nameStr](){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed non-symbol named class (with computed name method) property";
(function() {
    let values = nonSymbolValues;
    let nameStr = "name";
    function runTest(value) {
        let o = {    
            [value]: class bar { static [nameStr](){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class bar { static [nameStr](){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i]);
})();

section = "Object computed symbol anonymous class (with computed name method) property";
(function() {
    let values = symbolValues;
    let nameStr = "name";
    function runTest(value, expectedName) {
        let o = {    
            [value]: class { static [nameStr](){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class { static [nameStr](){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v, values[i].name);
})();

section = "Object computed symbol named class (with computed name method) property";
(function() {
    let values = symbolValues;
    let nameStr = "name";
    function runTest(value) {
        let o = {    
            [value]: class bar { static [nameStr](){} },
        }
        shouldBe("typeof ", "o[" + toString(value) + "].name", typeof o[value].name, "function");
        shouldBe("toString of ", "o[" + toString(value) + "].name", o[value].toString(), "class bar { static [nameStr](){} }");

        let bound = o[value].bind({});
        test(bound, "bound ", "function () { [native code] }");
    }
    for (var i = 0; i < values.length; i++)
        runTest(values[i].v);
})();

if (failureCount)
    throw Error("Found " + failureCount + " failures:\n" + failures);
