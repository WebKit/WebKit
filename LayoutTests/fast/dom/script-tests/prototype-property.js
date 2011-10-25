description("Make sure the 'prototype' property on generated Web IDL interfaces is { DontDelete | ReadOnly }.");

function tryToDeletePrototype(o) {
    var orig = o.prototype;
    delete o.prototype;
    var ret = o.prototype != orig;
    o.prototype = orig;
    return ret;
}

function tryToSetPrototype(o, value) {
    var orig = o.prototype;
    o.prototype = value;
    var ret = o.prototype != orig;
    o.prototype = orig;
    return ret;
}

shouldBe("tryToDeletePrototype(window.HTMLElement)", "false");
shouldBe("tryToSetPrototype(window.HTMLElement, null)", "false");
shouldBe("tryToSetPrototype(window.HTMLElement, undefined)", "false");
shouldBe("tryToSetPrototype(window.HTMLElement, 1)", "false");
shouldBe("tryToSetPrototype(window.HTMLElement, window.Object.prototype)", "false");
