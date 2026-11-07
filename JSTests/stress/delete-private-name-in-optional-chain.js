function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

function throwsSyntaxError(source) {
    try {
        (0, eval)(source);
    } catch (error) {
        return error instanceof SyntaxError;
    }
    return false;
}

const declarations = [
    "#x = 1;",
    "#x() {}",
    "get #x() { return 1; }",
    "set #x(v) {}",
    "static #x = 1;",
    "static #x() {}",
];

const wrappers = [
    (declaration, use) => `class C { ${declaration} m(o, y) { ${use}; } }`,
    (declaration, use) => `class C { m(o, y) { ${use}; } ${declaration} }`,
    (declaration, use) => `class C { ${declaration} m(o, y) { return () => { ${use}; }; } }`,
    (declaration, use) => `class C { ${declaration} m(o, y) { class D { n() { ${use}; } } } }`,
    (declaration, use) => `class C { ${declaration} m(o, y, z = (${use})) {} }`,
    (declaration, use) => `class C { ${declaration} f = (${use}); }`,
    (declaration, use) => `class C { ${declaration} f = (o, y) => (${use}); }`,
    (declaration, use) => `class C { ${declaration} static f = (o, y) => (${use}); }`,
    (declaration, use) => `class C { ${declaration} static { (o, y) => (${use}); } }`,
    (declaration, use) => `class C { ${declaration} [(o, y) => (${use})]() {} }`,
    (declaration, use) => `function f() { class C { ${declaration} [(o, y) => (${use})]() {} } }`,
    (declaration, use) => `class C { ${declaration} static m(o, y) { eval(${JSON.stringify(use)}); } } C.m();`,
    (declaration, use) => `class C { ${declaration} static m(o, y) { eval(${JSON.stringify(`(function () { ${use}; })`)}); } } C.m();`,
];

const invalid = [
    "delete o?.#x",
    "delete o?.y.#x",
    "delete o?.[y].#x",
    "delete o?.().#x",
    "delete o?.y().#x",
    "delete o?.y?.#x",
    "delete o.y?.#x",
    "delete o[y]?.#x",
    "delete o()?.#x",
    "delete this?.#x",
    "delete o?.#x.#x",
    "delete o?.#x?.#x",
    "delete o.#x?.#x",
    "delete (o?.y)?.#x",
    "delete (o?.#x)",
    "delete ((o?.y.#x))",
    "delete delete o?.#x",
    "typeof delete o?.#x",
    "delete o?.#x ?? y",
    "[delete o?.#x]",
    "() => delete o?.#x",
];

const valid = [
    "o?.#x",
    "typeof o?.#x",
    "delete o?.x",
    "delete o?.['#x']",
    "delete o?.#x.y",
    "delete o?.#x?.y",
    "delete o?.#x[y]",
    "delete o?.#x?.[y]",
    "delete o?.#x()",
    "delete o?.#x?.()",
    "delete o.#x?.y",
    "delete (0, o?.#x)",
    "delete (o?.#x).y",
    "delete (o?.#x)?.y",
    "delete (o?.#x ?? y)",
    "delete (y ? o?.#x : y)",
    "delete typeof o?.#x",
    "delete [o?.#x]",
    "delete o?.[o?.#x]",
    "delete o?.y(o?.#x)",
];

for (const wrapper of wrappers) {
    for (const declaration of declarations) {
        shouldBe(throwsSyntaxError(wrapper(declaration, invalid[0])), true);
        shouldBe(throwsSyntaxError(wrapper(declaration, valid[0])), false);
    }
    for (const use of invalid)
        shouldBe(throwsSyntaxError(wrapper(declarations[0], use)), true);
    for (const use of valid)
        shouldBe(throwsSyntaxError(wrapper(declarations[0], use)), false);
}

class C {
    #x = { y: 1 };

    static deleteY(o) {
        return delete o?.#x.y;
    }

    static deletePublic(o) {
        return delete o?.["#x"];
    }

    static hasY(o) {
        return "y" in o.#x;
    }
}

const c = new C;
c["#x"] = 2;
shouldBe(C.deleteY(null), true);
shouldBe(C.deletePublic(undefined), true);
shouldBe(C.hasY(c), true);
shouldBe(C.deleteY(c), true);
shouldBe(C.hasY(c), false);
shouldBe(Object.hasOwn(c, "#x"), true);
shouldBe(C.deletePublic(c), true);
shouldBe(Object.hasOwn(c, "#x"), false);
shouldBe(C.hasY(c), false);
