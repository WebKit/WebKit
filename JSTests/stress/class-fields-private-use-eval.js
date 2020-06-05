//@ requireOptions("--usePrivateClassFields=1", "--usePublicClassFields=1")

function assert(a, message) {
    if (!a)
        throw new Error(message);
}

let inst = new class {
  a = eval("(i) => i.#b");

  #b = {};
};

inst.a(inst).x = 1;
assert(inst.a(inst).x == 1);
