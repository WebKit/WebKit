//@ requireOptions("--poisonDeadOSRExitVariables=1")

const b = () => {};

function inlinee() {
    const a = {};
    const c = a instanceof b;
    try {

    } catch {
        return c;
    }
}

function opt() {
    return inlinee();
}

function main() {
    for (let i = 0; i < 1000; i++) {
        b.prototype = {};
        opt();
    }

    b.prototype = null;

    opt().foo();
}

let exn;
try {
    main();
} catch (e) {
    exn = e;
}
if (exn?.constructor !== TypeError && exn?.message.indexOf("instanceof") < 0) {
    throw new Error("expected TypeError in instanceof");
}
