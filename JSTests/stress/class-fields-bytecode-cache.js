//@ runBytecodeCache("--useClassFields=true")

function assert(a, e) {
    if (a !== e)
        throw new Erro("Expected: " + e + " but got: " + a);
}

class C{
    foo() {
        return this.f;
    }

    f = 15;
}

let c = new C();
let r = c.foo();
assert(r, 15);

