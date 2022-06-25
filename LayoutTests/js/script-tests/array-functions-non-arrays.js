description(
"Test some array functions on non-array objects."
);

function properties(object, extraName1, extraName2, extraName3)
{
    var string = "";

    // destructive, lists properties
    var names = [];
    var enumerables = {};
    for (propertyName in object) {
        names.push(propertyName);
        enumerables[propertyName] = 1;
    }
    names.push("length");
    names.push(extraName1);
    names.push(extraName2);
    names.push(extraName3);
    names.sort();

    var propertyStrings = [];
    for (i = 0; i < names.length; ++i) {
        var name = names[i];
        if (name == names[i - 1])
            continue;
        if (!(name in object))
            continue;

        var propertyString = name + ":" + object[name];

        var flags = [];
        if (!object.hasOwnProperty(name))
            flags.push("FromPrototype");
        if (!enumerables[name])
            flags.push("DontEnum");
        if (name != "length") {
            try {
                object[name] = "ReadOnlyTestValue";
            } catch (e) {
            }
            if (object[name] != "ReadOnlyTestValue")
                flags.push("ReadOnly");
        }
        delete object[name];
        if (object.hasOwnProperty(name))
            flags.push("DontDelete");

        flags.sort();

        if (flags.length)
            propertyString += "(" + flags.join(", ") + ")";

        propertyStrings.push(propertyString);
    }
    return propertyStrings.join(", ");
}

var x;

var oneItemPrototype = { length:1, 0:'a' };
function OneItemConstructor()
{
}
OneItemConstructor.prototype = oneItemPrototype;

var twoItemPrototype = { length:2, 0:'b', 1:'a' };
function TwoItemConstructor()
{
}
TwoItemConstructor.prototype = twoItemPrototype;

shouldBe("properties(['b', 'a'])", "'0:b, 1:a, length:2(DontDelete, DontEnum)'");
shouldBe("properties({ length:2, 0:'b', 1:'a' })", "'0:b, 1:a, length:2'");

shouldBe("properties(new OneItemConstructor)", "'0:a(FromPrototype), length:1(FromPrototype)'");
shouldBe("properties(new TwoItemConstructor)", "'0:b(FromPrototype), 1:a(FromPrototype), length:2(FromPrototype)'");

shouldBe("Array.prototype.toString.call({})", '"' + ({}).toString() + '"');
shouldBe("Array.prototype.toString.call(new Date)", '"' + Object.prototype.toString.call(new Date) + '"');
shouldBe("Array.prototype.toString.call({sort: function() { return 'sort' }})", '"' + Object.prototype.toString.call({}) + '"');
shouldBe("Array.prototype.toString.call({join: function() { return 'join' }})", '"join"');
shouldBe("Array.prototype.toString.call({__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', length: 3})", '"a,b,c"');
shouldBe("({__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', length: 3}).toString()", '"a,b,c"');
shouldBe("Array.prototype.toString.call({__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', length: 3, join: function() { return 'join' }})", '"join"');
shouldBe("({__proto__: Array.prototype, 0: 'a', 1: 'b', 2: 'c', length: 3, join: function() { return 'join' }}).toString()", '"join"');
Number.prototype.join = function() { return "Number.prototype.join:" + this; }
shouldBe("Array.prototype.toString.call(42)", '"Number.prototype.join:42"');
var arrayJoin = Array.prototype.join;
Array.prototype.join = function() { return 'array-join' };
shouldBe("[0, 1, 2].toString()", '"array-join"');
Array.prototype.join = arrayJoin;

shouldBe("Array.prototype.toLocaleString.call({})", '"' + '"');

shouldBe("Array.prototype.concat.call(x = { length:2, 0:'b', 1:'a' })", "[x]");

shouldBe("Array.prototype.join.call({})", "''");
shouldBe("Array.prototype.join.call(['b', 'a'])", "'b,a'");
shouldBe("Array.prototype.join.call({ length:2, 0:'b', 1:'a' })", "'b,a'");
shouldBe("Array.prototype.join.call(new TwoItemConstructor)", "'b,a'");

shouldBe("Array.prototype.pop.call({})", "undefined");
shouldBe("Array.prototype.pop.call({ length:2, 0:'b', 1:'a' })", "'a'");
shouldBe("Array.prototype.pop.call({ length:2, 0:'b', 1:'a' })", "'a'");
shouldBe("Array.prototype.pop.call(new TwoItemConstructor)", "'a'");
shouldBe("Array.prototype.pop.call(x = {}); properties(x)", "'length:0'");
shouldBe("Array.prototype.pop.call(x = ['b', 'a']); properties(x)", "'0:b, length:1(DontDelete, DontEnum)'");
shouldBe("Array.prototype.pop.call(x = { length:2, 0:'b', 1:'a' }); properties(x)", "'0:b, length:1'");
shouldBe("Array.prototype.pop.call(x = new TwoItemConstructor); properties(x)", "'0:b(FromPrototype), 1:a(FromPrototype), length:1'");

shouldBe("Array.prototype.push.call({})", "0");
shouldBe("Array.prototype.push.call(['b', 'a'])", "2");
shouldBe("Array.prototype.push.call({ length:2, 0:'b', 1:'a' })", "2");
shouldBe("Array.prototype.push.call(new TwoItemConstructor)", "2");
shouldBe("Array.prototype.push.call(x = {}); properties(x)", "'length:0'");
shouldBe("Array.prototype.push.call(x = ['b', 'a']); properties(x)", "'0:b, 1:a, length:2(DontDelete, DontEnum)'");
shouldBe("Array.prototype.push.call(x = { length:2, 0:'b', 1:'a' }); properties(x)", "'0:b, 1:a, length:2'");
shouldBe("Array.prototype.push.call(x = new TwoItemConstructor); properties(x)", "'0:b(FromPrototype), 1:a(FromPrototype), length:2'");
shouldBe("Array.prototype.push.call({}, 'c')", "1");
shouldBe("Array.prototype.push.call(['b', 'a'], 'c')", "3");
shouldBe("Array.prototype.push.call({ length:2, 0:'b', 1:'a' }, 'c')", "3");
shouldBe("Array.prototype.push.call(new TwoItemConstructor, 'c')", "3");
shouldBe("Array.prototype.push.call(x = {}, 'c'); properties(x)", "'0:c, length:1'");
shouldBe("Array.prototype.push.call(x = ['b', 'a'], 'c'); properties(x)", "'0:b, 1:a, 2:c, length:3(DontDelete, DontEnum)'");
shouldBe("Array.prototype.push.call(x = { length:2, 0:'b', 1:'a' }, 'c'); properties(x)", "'0:b, 1:a, 2:c, length:3'");
shouldBe("Array.prototype.push.call(x = new TwoItemConstructor, 'c'); properties(x)", "'0:b(FromPrototype), 1:a(FromPrototype), 2:c, length:3'");

shouldBe("properties(Array.prototype.reverse.call({}))", "''");
shouldBe("properties(Array.prototype.reverse.call(['b', 'a']))", "'0:a, 1:b, length:2(DontDelete, DontEnum)'");
shouldBe("properties(Array.prototype.reverse.call({ length:2, 0:'b', 1:'a' }))", "'0:a, 1:b, length:2'");
shouldBe("properties(Array.prototype.reverse.call(new OneItemConstructor))", "'0:a(FromPrototype), length:1(FromPrototype)'");
shouldBe("properties(Array.prototype.reverse.call(new TwoItemConstructor))", "'0:a, 1:b, length:2(FromPrototype)'");

shouldBe("Array.prototype.shift.call({})", "undefined");
shouldBe("Array.prototype.shift.call(['b', 'a'])", "'b'");
shouldBe("Array.prototype.shift.call({ length:2, 0:'b', 1:'a' })", "'b'");
shouldBe("Array.prototype.shift.call(new TwoItemConstructor)", "'b'");
shouldBe("Array.prototype.shift.call(x = {}); properties(x)", "'length:0'");
shouldBe("Array.prototype.shift.call(x = ['b', 'a']); properties(x)", "'0:a, length:1(DontDelete, DontEnum)'");
shouldBe("Array.prototype.shift.call(x = { length:2, 0:'b', 1:'a' }); properties(x)", "'0:a, length:1'");
shouldBe("Array.prototype.shift.call(x = new TwoItemConstructor); properties(x)", "'0:a, 1:a(FromPrototype), length:1'");

shouldBe("Array.prototype.slice.call({}, 0, 1)", "[]");
shouldBe("Array.prototype.slice.call(['b', 'a'], 0, 1)", "['b']");
shouldBe("Array.prototype.slice.call({ length:2, 0:'b', 1:'a' }, 0, 1)", "['b']");
shouldBe("Array.prototype.slice.call(new TwoItemConstructor, 0, 1)", "['b']");

shouldBe("properties(Array.prototype.sort.call({}))", "''");
shouldBe("properties(Array.prototype.sort.call(['b', 'a']))", "'0:a, 1:b, length:2(DontDelete, DontEnum)'");
shouldBe("properties(Array.prototype.sort.call({ length:2, 0:'b', 1:'a' }))", "'0:a, 1:b, length:2'");
shouldBe("properties(Array.prototype.sort.call(new OneItemConstructor))", "'0:a(FromPrototype), length:1(FromPrototype)'");
shouldBe("properties(Array.prototype.sort.call(new TwoItemConstructor))", "'0:a, 1:b, length:2(FromPrototype)'");

shouldBe("Array.prototype.splice.call({}, 0, 1)", "[]");
shouldBe("Array.prototype.splice.call(['b', 'a'], 0, 1)", "['b']");
shouldBe("Array.prototype.splice.call({ length:2, 0:'b', 1:'a' }, 0, 1)", "['b']");
shouldBe("Array.prototype.splice.call(new TwoItemConstructor, 0, 1)", "['b']");
shouldBe("Array.prototype.splice.call(x = {}, 0, 1); properties(x)", "'length:0'");
shouldBe("Array.prototype.splice.call(x = ['b', 'a'], 0, 1); properties(x)", "'0:a, length:1(DontDelete, DontEnum)'");
shouldBe("Array.prototype.splice.call(x = { length:2, 0:'b', 1:'a' }, 0, 1); properties(x)", "'0:a, length:1'");
shouldBe("Array.prototype.splice.call(x = new TwoItemConstructor, 0, 1); properties(x)", "'0:a, 1:a(FromPrototype), length:1'");

shouldBe("Array.prototype.unshift.call({})", "0");
shouldBe("Array.prototype.unshift.call(['b', 'a'])", "2");
shouldBe("Array.prototype.unshift.call({ length:2, 0:'b', 1:'a' })", "2");
shouldBe("Array.prototype.unshift.call(new TwoItemConstructor)", "2");
shouldBe("Array.prototype.unshift.call(x = {}); properties(x)", "'length:0'");
shouldBe("Array.prototype.unshift.call(x = ['b', 'a']); properties(x)", "'0:b, 1:a, length:2(DontDelete, DontEnum)'");
shouldBe("Array.prototype.unshift.call(x = { length:2, 0:'b', 1:'a' }); properties(x)", "'0:b, 1:a, length:2'");
shouldBe("Array.prototype.unshift.call(x = new TwoItemConstructor); properties(x)", "'0:b(FromPrototype), 1:a(FromPrototype), length:2'");
shouldBe("Array.prototype.unshift.call({}, 'c')", "1");
shouldBe("Array.prototype.unshift.call(['b', 'a'], 'c')", "3");
shouldBe("Array.prototype.unshift.call({ length:2, 0:'b', 1:'a' }, 'c')", "3");
shouldBe("Array.prototype.unshift.call(new TwoItemConstructor, 'c')", "3");
shouldBe("Array.prototype.unshift.call(x = {}, 'c'); properties(x)", "'0:c, length:1'");
shouldBe("Array.prototype.unshift.call(x = ['b', 'a'], 'c'); properties(x)", "'0:c, 1:b, 2:a, length:3(DontDelete, DontEnum)'");
shouldBe("Array.prototype.unshift.call(x = { length:2, 0:'b', 1:'a' }, 'c'); properties(x)", "'0:c, 1:b, 2:a, length:3'");
shouldBe("Array.prototype.unshift.call(x = new TwoItemConstructor, 'c'); properties(x)", "'0:c, 1:b, 2:a, length:3'");

// FIXME: Add tests for every, forEach, some, indexOf, lastIndexOf, filter, and map
debug("Array-like object with invalid lengths");
var throwError = function throwError() {
    throw new Error("should not reach here");
};
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: 0 }; Array.prototype.forEach.call(obj, throwError)");
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: -0 }; Array.prototype.forEach.call(obj, throwError)");
shouldBeUndefined("var obj = { 0: 1, 1: 2, 2: 3, length: -3 }; Array.prototype.forEach.call(obj, throwError)");
shouldBe("var obj = { 0: 1, 1: 2, 2: 3, length: 0 }; Array.prototype.map.call(obj, throwError)", "[]");
shouldBe("var obj = { 0: 1, 1: 2, 2: 3, length: -0 }; Array.prototype.map.call(obj, throwError)", "[]");
shouldBe("var obj = { 0: 1, 1: 2, 2: 3, length: -3 }; Array.prototype.map.call(obj, throwError)", "[]");
shouldBeFalse("var obj = { 0: 1, 1: 2, 2: 3, length: 0 }; Array.prototype.some.call(obj, throwError)");
shouldBeFalse("var obj = { 0: 1, 1: 2, 2: 3, length: -0 }; Array.prototype.some.call(obj, throwError)");
shouldBeFalse("var obj = { 0: 1, 1: 2, 2: 3, length: -3 }; Array.prototype.some.call(obj, throwError)");
