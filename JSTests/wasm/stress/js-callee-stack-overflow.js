//@ requireOptions("--maxPerThreadStackUsage=524288")

import Builder from "../Builder.js";

const builder = (new Builder())
    .Type().End()
    .Import().Function("env", "reenter", { params: [], ret: "void" }).End()
    .Function().End()
    .Export()
        .Function("run")
    .End()
    .Code()
        .Function("run", { params: [], ret: "void" })
            .Call(0)
        .End()
    .End();

let instance;
function reenter()
{
    instance.exports.run();
}

instance = new WebAssembly.Instance(new WebAssembly.Module(builder.WebAssembly().get()), { env: { reenter } });

try {
    instance.exports.run();
} catch (e) {
    if (!(e instanceof RangeError))
        throw e;
}

const identity = (new Builder())
    .Type().End()
    .Function().End()
    .Export()
        .Function("f")
    .End()
    .Code()
        .Function("f", { params: ["f64"], ret: "f64" })
            .GetLocal(0)
        .End()
    .End();

const numberInstance = new WebAssembly.Instance(new WebAssembly.Module(identity.WebAssembly().get()));
const rec = {
    valueOf() {
        return numberInstance.exports.f(rec);
    }
};
try {
    numberInstance.exports.f(rec);
} catch (e) {
    if (!(e instanceof RangeError))
        throw e;
}
