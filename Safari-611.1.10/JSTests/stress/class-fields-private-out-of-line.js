//@ requireOptions("--usePrivateClassFields=1")

function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

let klassSource = `
var C = class {
    assertOutOfLineValue() {
        assert(this.#f200, "foo");
    }
`;


for (let i = 0; i < 200; i++) {
    klassSource += "    #f" + i + ";\n";
}

klassSource += "    #f200 = \"foo\";\n}";

eval(klassSource);

for (let i = 0; i < 2; i++) {
    let c = new C();
    c.assertOutOfLineValue();
}

