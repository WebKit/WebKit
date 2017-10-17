description("basic tests for object literal shorthand construction");

function equivalent(o1, o2) {
    var keys1 = Object.keys(o1);
    var keys2 = Object.keys(o2);
    if (keys1.length !== keys2.length)
        return false;

    for (var i = 0; i < keys1.length; ++i) {
        if (keys1[i] !== keys2[i])
            return false;
    }

    for (var p in o1) {
        if (o1[p] !== o2[p])
            return false;
    }

    return true;
}

function testShorthandConstructionEquivalent(expr1, expr2) {
    shouldBeTrue("equivalent(" + expr1 + ", " + expr2 + ")");
}

function testShorthandConstructionNotEquivalent(expr1, expr2) {
    shouldBeTrue("!equivalent(" + expr1 + ", " + expr2 + ")");
}

a = 1;
b = 2;
c = 3;
t = true;
x = -0;
f = 123.456;
nul = null;
un = undefined;
fun = function myFunction() {}
foo = {property: "value"};
bar = [1, 2, 3];

shouldBeTrue("!!{a}");
shouldBeTrue("({a}).a === 1");
shouldBeTrue("({a}).a === a");
shouldBeTrue("({foo})['foo'] === foo");
shouldBeTrue("Object.getOwnPropertyDescriptor({a}, 'a').enumerable");
shouldBeTrue("Object.getOwnPropertyDescriptor({a}, 'a').configurable");
shouldBeTrue("Object.getOwnPropertyDescriptor({a}, 'a').writable");
shouldBe("Object.keys({a,b}).join()", '"a,b"');
shouldBe("Object.keys({b,a}).join()", '"b,a"');

testShorthandConstructionEquivalent("{a}", "{a:a}");
testShorthandConstructionEquivalent("{a}", "{a:a}");
testShorthandConstructionEquivalent("{a,}", "{a:a}");
testShorthandConstructionEquivalent("{a,a}", "{a:a}");
testShorthandConstructionEquivalent("{a,b}", "{a:a, b:b}");
testShorthandConstructionEquivalent("{ a , b }", "{a:a, b:b}");
testShorthandConstructionEquivalent("{a,b,}", "{a:a, b:b}");
testShorthandConstructionEquivalent("{a,b,a}", "{a:a, b:b}");
testShorthandConstructionEquivalent("{b,a,a}", "{b:b, a:a}");
testShorthandConstructionNotEquivalent("{a}", "{b:a}");
testShorthandConstructionNotEquivalent("{b}", "{a:b}");
testShorthandConstructionNotEquivalent("{a,b}", "{a:b, b:a}");

testShorthandConstructionEquivalent("{foo}", "{foo:foo}");
testShorthandConstructionEquivalent("{foo}", "{foo:foo}");
testShorthandConstructionEquivalent("{foo,}", "{foo:foo}");
testShorthandConstructionEquivalent("{foo,foo}", "{foo:foo}");
testShorthandConstructionEquivalent("{foo,bar}", "{foo:foo, bar:bar}");
testShorthandConstructionEquivalent("{ foo , bar }", "{foo:foo, bar:bar}");
testShorthandConstructionEquivalent("{foo,bar,}", "{foo:foo, bar:bar}");
testShorthandConstructionEquivalent("{foo,bar,foo}", "{foo:foo, bar:bar}");
testShorthandConstructionEquivalent("{bar,foo,foo}", "{bar:bar, foo:foo}");
testShorthandConstructionEquivalent("{foo,bar,foo}", "{foo:foo, bar:bar}");
testShorthandConstructionEquivalent("{bar,foo,foo}", "{bar:bar, foo:foo}");
testShorthandConstructionNotEquivalent("{foo}", "{bar:foo}");
testShorthandConstructionNotEquivalent("{bar}", "{foo:bar}");
testShorthandConstructionNotEquivalent("{foo,bar}", "{foo:bar, bar:foo}");

testShorthandConstructionEquivalent("{a, b:b, c}", "{a, b, c}");
testShorthandConstructionEquivalent("{a:a, b, c:c}", "{a, b, c}");
testShorthandConstructionEquivalent("{a, b, c:c}", "{a, b, c}");
testShorthandConstructionEquivalent("{'a':a, b, c:c}", "{a, b, c}");

testShorthandConstructionEquivalent("{nest:{a}}.nest", "{nest: {a:a}}.nest");
testShorthandConstructionEquivalent("{nest:[{a}]}.nest[0]", "{nest: [{a:a}]}.nest[0]");
testShorthandConstructionEquivalent("[{nest:[{a}]}][0].nest[0]", "[{nest: [{a:a}]}][0].nest[0]");
testShorthandConstructionEquivalent("{a,b,t,x,f,nul,un,fun,foo,bar}", "{a:a, b:b, t:t, x:x, f:f, nul:null, un:un, fun:fun, foo:foo, bar:bar}");

testShorthandConstructionEquivalent("{eval}", "{eval: eval}");

shouldThrow("({noSuchIdentifier})");
shouldThrow("({a,noSuchIdentifier})");
shouldThrow("({noSuchIdentifier,a})");
shouldThrow("({a,b,noSuchIdentifier})");

shouldThrow("({get})"); // Shorthand, not a getter, but ReferenceError.
shouldThrow("({set})"); // Shorthand, not a setter, but ReferenceError.
get = 1;
set = 2;
testShorthandConstructionEquivalent("{get}", "{get: 1}");
testShorthandConstructionEquivalent("{set}", "{set: 2}");

// Getter/Setter syntax still works.
shouldBeTrue("({get x(){ return true; }}).x");
shouldBeTrue("({get 'x'(){ return true; }}).x");
shouldBeTrue("({get 42(){ return true; }})['42']");
shouldBeTrue("!!Object.getOwnPropertyDescriptor({set x(value){}}, 'x').set");
shouldBeTrue("!!Object.getOwnPropertyDescriptor({set 'x'(value){}}, 'x').set");
shouldBeTrue("!!Object.getOwnPropertyDescriptor({set 42(value){}}, '42').set");

// __proto__ shorthand should not modify the prototype.
shouldThrow("this.__proto__ = []");
shouldBeFalse("({__proto__: this.__proto__}) instanceof Array");
shouldThrow("__proto__ = []", '"TypeError: Object.prototype.__proto__ called on null or undefined"');
shouldThrow("({__proto__: __proto__}) instanceof Array", '"TypeError: undefined is not an object (evaluating \'__proto__\')"');

// Keywords - Syntax Errors
debug("SyntaxErrors");
shouldThrow(`({break})`);
shouldThrow(`({case})`);
shouldThrow(`({catch})`);
shouldThrow(`({class})`);
shouldThrow(`({const})`);
shouldThrow(`({continue})`);
shouldThrow(`({debugger})`);
shouldThrow(`({default})`);
shouldThrow(`({delete})`);
shouldThrow(`({do})`);
shouldThrow(`({else})`);
shouldThrow(`({enum})`);
shouldThrow(`({export})`);
shouldThrow(`({extends})`);
shouldThrow(`({false})`);
shouldThrow(`({finally})`);
shouldThrow(`({for})`);
shouldThrow(`({function})`);
shouldThrow(`({if})`);
shouldThrow(`({import})`);
shouldThrow(`({in})`);
shouldThrow(`({instanceof})`);
shouldThrow(`({new})`);
shouldThrow(`({null})`);
shouldThrow(`({return})`);
shouldThrow(`({super})`);
shouldThrow(`({switch})`);
shouldThrow(`({throw})`);
shouldThrow(`({true})`);
shouldThrow(`({try})`);
shouldThrow(`({typeof})`);
shouldThrow(`({var})`);
shouldThrow(`({void})`);
shouldThrow(`({while})`);
shouldThrow(`({with})`);

// Sometimes Keywords - yield and await become keywords in some cases
debug("Contextual Keywords")
shouldThrow(`({let})`); // ReferenceError
shouldThrow(`({async})`); // ReferenceError
shouldThrow(`({yield})`); // ReferenceError
shouldThrow(`({await})`); // ReferenceError
shouldThrow(`"use strict"; ({let}) }`); // SyntaxError
shouldThrow(`function* generator() { ({yield}) }`); // SyntaxError
shouldThrow(`async function func() { ({await}) }`); // SyntaxError
