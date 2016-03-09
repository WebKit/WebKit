// isWhiteSpace() and stripSpaces() are borrowed from
// kde/script-tests/inbuilt_function_tostring.js and modified.

let section;
let failures = "";
let failureCount = 0;

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
    if (typeof(actual) !== typeof(expected)) {
        failures += ("   " + section + ": " + desc + "'" + funcName + "': typeof expected: " + typeof(expected) + ", typeof actual: " + typeof(actual) + "\n");
        failures += ("       expected: '" + expected + "', actual: '" + actual + "'\n");
        failureCount++;
    } else if (typeof(actual) !== typeof(expected) || actual !== expected) {
        failures += ("   " + section + ": " + desc + "'" + funcName + "': expected: '" + expected + "', actual: '" + actual + "'\n");
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

// FIXME: Uncomment these when we've added support for Function.name of computed properties.
// section = "Object computed string property";
// {
//     let str1 = "foo";
//     let str2 = "";
//     let o = {    
//         [str1]: function() {},
//         [str2]: function() {}
//     };
//     test(o[str1], "foo", "function() {}");
//     test(o[str2], "", "function() {}");
// }
// 
// let sym1 = Symbol("foo");
// let sym2 = Symbol();

// section = "Object computed symbol property";
// {
//     let o = {    
//         [sym1]: function() {},
//         [sym2]: function() {}
//     };
//     test(o[sym1], "[foo]", "function() {}");
//     test(o[sym2], "", "function() {}");
// }

// section = "Object computed symbol property with shorthand function";
// {
//     let o = {
//         [sym1]() {},
//         [sym2]() {}
//     };
//     test(o[sym1], "[foo]", "function() {}");
//     test(o[sym2], "", "function() {}");
// }

// section = "Object computed symbol property with get/set function";
// {
//     let o = {
//         get [sym1]() {},
//         set [sym1](x) {},
//         get [sym2]() {},
//         set [sym2](x) {}
//     };
//     let desc = Object.getOwnPropertyDescriptor(o, sym1);
//     test(desc.get, "get [foo]", "function() {}");
//     test(desc.set, "set [foo]", "function(x) {}");
// 
//     desc = Object.getOwnPropertyDescriptor(o, sym2);
//     test(desc.get, "get ", "function() {}");
//     test(desc.set, "set ", "function(x) {}");
// }

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
    test(desc2.get, "get ", "function() {}");
    test(desc2.set, "set ", "function(x) {}");

    let o3 = { get [100]() {}, set [100](x){} };
    let desc3 = Object.getOwnPropertyDescriptor(o3, 100);
    test(desc3.get, "get ", "function() {}");
    test(desc3.set, "set ", "function(x) {}");

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
        test(bound1, "bound get ", "function get () { [native code] }");
        bound2 = desc2.set.bind(o);
        test(bound2, "bound set ", "function set () { [native code] }");

        bound1 = desc3.get.bind(o);
        test(bound1, "bound get ", "function get () { [native code] }");
        bound2 = desc3.set.bind(o);
        test(bound2, "bound set ", "function set () { [native code] }");
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

if (failureCount)
    throw Error("Found " + failureCount + " failures:\n" + failures);
