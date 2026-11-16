function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

// Class methods (strict, no "prototype" property) and object-literal shorthand methods
// use dedicated method structures. Function#bind on them should be strength-reduced
// to NewBoundFunction with lazily materialized .name/.length, and stay correct.
class Component {
    constructor() { this.state = 40; }
    handleClick(a, b) { return this.state + a + b; }
}

var sloppyHolder = {
    handle(a, b, c) { return this === sloppyHolder ? a + b + c : -1; },
};

var component = new Component();

function bindClassMethod() {
    return component.handleClick.bind(component, 1);
}
noInline(bindClassMethod);

function bindShorthandMethod() {
    return sloppyHolder.handle.bind(sloppyHolder);
}
noInline(bindShorthandMethod);

for (var i = 0; i < testLoopCount; ++i) {
    var f = bindClassMethod();
    shouldBe(f(1), 42);
    shouldBe(f.name, "bound handleClick");
    shouldBe(f.length, 1);

    var g = bindShorthandMethod();
    shouldBe(g(1, 2, 3), 6);
    shouldBe(g.name, "bound handle");
    shouldBe(g.length, 3);
}

// Methods are not constructors, and neither are their bound versions.
shouldThrow(() => new (bindClassMethod())(), "TypeError: function is not a constructor (evaluating 'new (bindClassMethod())()')");
shouldThrow(() => new (bindShorthandMethod())(), "TypeError: function is not a constructor (evaluating 'new (bindShorthandMethod())()')");

// Once .name/.length are reified (the structure transitions), bind must observe the modified values.
class Modified {
    method(a, b) { return a; }
}
var modified = new Modified();
Object.defineProperty(modified.method, "name", { value: "renamed" });
Object.defineProperty(modified.method, "length", { value: 7 });

function bindModifiedMethod() {
    return modified.method.bind(null, 0);
}
noInline(bindModifiedMethod);

for (var i = 0; i < testLoopCount; ++i) {
    var h = bindModifiedMethod();
    shouldBe(h.name, "bound renamed");
    shouldBe(h.length, 6);
}
