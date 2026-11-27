function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function customTestPrototype(keepCustomValues) {
    var prototype = $vm.createCustomTestGetterSetter();
    for (var name of Object.getOwnPropertyNames(prototype)) {
        if (name.startsWith("customValue") !== keepCustomValues)
            delete prototype[name];
    }
    return prototype;
}

function TwoProperties(a, b) {
    this.a = a;
    this.b = b;
}

function WithAccessors(a, b) {
    this.a = a;
    this.b = b;
}
WithAccessors.prototype = {
    get x() { return this.a; },
    set x(v) { this.a = v; },
    get y() { return this.b; },
    get z() { return 0; },
};

function WithCustomAccessors(a, b) {
    this.a = a;
    this.b = b;
}
WithCustomAccessors.prototype = customTestPrototype(false);

class WithClassAccessors {
    constructor(a, b) {
        this.a = a;
        this.b = b;
    }
    get x() { return this.a; }
    set x(v) { this.a = v; }
    get y() { return this.b; }
    get z() { return 0; }
}

function WithReadOnlyValues(a, b) {
    this.a = a;
    this.b = b;
}
Object.defineProperty(WithReadOnlyValues.prototype, "x", { value: 1 });
Object.defineProperty(WithReadOnlyValues.prototype, "y", { value: 2 });
Object.defineProperty(WithReadOnlyValues.prototype, "z", { value: 3 });

function OneProperty(a) {
    this.a = a;
}

function WithCustomValues(a) {
    this.a = a;
}
WithCustomValues.prototype = customTestPrototype(true);

function test(i) {
    var expected = $vm.inlineCapacity(new TwoProperties(i, i));
    shouldBe($vm.inlineCapacity(new WithAccessors(i, i)), expected);
    shouldBe($vm.inlineCapacity(new WithCustomAccessors(i, i)), expected);
    shouldBe($vm.inlineCapacity(new WithClassAccessors(i, i)), expected);
    shouldBe($vm.inlineCapacity(new WithReadOnlyValues(i, i)), expected);
    shouldBe($vm.inlineCapacity(new WithCustomValues(i)) > $vm.inlineCapacity(new OneProperty(i)), true);
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    test(i);
