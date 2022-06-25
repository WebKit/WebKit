Object.defineProperty(Object.prototype, "__proto__", {get: undefined});

var A = class A { };
var B = class B extends A { };
var C = class C extends B { constructor() { super(); } };

noInline(C);

(function() {
    var x;
    for (var i = 0; i < 1e5; ++i)
        x = new C(false);
})();

var D = class D extends A { constructor() {
    super(...arguments);
    return function () { return arguments; }
} };
var E = class E extends D { constructor() { super(); } };

noInline(E);

(function() {
    var x;
    for (var i = 0; i < 1e5; ++i)
        x = new C(false);
})();
