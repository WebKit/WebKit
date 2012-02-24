description("Tests Function.bind.");

var result;

function F(x, y)
{
    result = this + " -> x:" + x + ", y:" + y;
}

G = F.bind("'a'", "'b'");
H = G.bind("'Cannot rebind this!'", "'c'");

F(1,2);
shouldBe("result", '"[object Window] -> x:1, y:2"');
G(1,2);
shouldBe("result", '"\'a\' -> x:\'b\', y:1"');
H(1,2);
shouldBe("result", '"\'a\' -> x:\'b\', y:\'c\'"');

var f = new F(1,2);
shouldBe("result", '"[object Object] -> x:1, y:2"');
var g = new G(1,2);
shouldBe("result", '"[object Object] -> x:\'b\', y:1"');
var h = new H(1,2);
shouldBe("result", '"[object Object] -> x:\'b\', y:\'c\'"');

// Objects f & g & h are all actually instances of F, G's HasInstance method
// actually is checking whether the argument is an instance of F, and H's
// HasInstance calls G's which calls F's!
shouldBeTrue('f instanceof F');
shouldBeTrue('f instanceof G');
shouldBeTrue('f instanceof H');
shouldBeTrue('g instanceof F');
shouldBeTrue('g instanceof G');
shouldBeTrue('g instanceof H');
shouldBeTrue('h instanceof F');
shouldBeTrue('h instanceof G');
shouldBeTrue('h instanceof H');

// Bound functions don't have a 'prototype' property.
shouldBeTrue('"prototype" in F');
shouldBeFalse('"prototype" in G');
shouldBeFalse('"prototype" in H');

// The object passed to bind as 'this' must be callable.
shouldThrow('Function.bind.call(undefined)');
// Objects that allow call but not construct can be bound, but should throw if used with new.
var abcAt = String.prototype.charAt.bind("abc");
shouldBe('abcAt(1)', '"b"');
shouldThrow('new abcAt(1)');

// This exposes a bug in our implementation of instanceof. The prototype property should not
// be accessed until after we have established the default HasInstance is being executed.
// https://bugs.webkit.org/show_bug.cgi?id=68656
var boundFunctionPrototypeAccessed = false;
P = F.bind(1,2);
Object.defineProperty(P, 'prototype', { get:function(){ boundFunctionPrototypeAccessed = true; } });
f instanceof P;
shouldBeFalse('boundFunctionPrototypeAccessed');

shouldBe('Function.bind.length', '1');
