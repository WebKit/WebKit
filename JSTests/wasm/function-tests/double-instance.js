import Builder from '../Builder.js'
import * as assert from '../assert.js'

// The call sequence is as follows:
//
// js -js2wasm-> i1.callAnother()
//               -wasm2wasm-> i0.callAnother()
//                            -wasm2js-> i1.boom()
//                                       -calldirect-> i1.doStackCheck()
//                                                     -calldirect-> dummy()
//               -calldirect-> i1.doStackCheck()
//                             -calldirect-> dummy()
//
// We therefore have i1 indirectly calling into itself, through another
// instance. When returning its cached stack limit should still be valid, but
// our implementation used to set it to UINTPTR_MAX causing an erroneous stack
// check failure at the second doStackCheck() call.

const builder = new Builder()
    .Type()
    .End()
    .Import()
        .Function("imp", "boom", {params:["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32"], ret:"i32"})
        .Function("imp", "callAnother", {params:["i32"], ret:"i32"})
    .End()
    .Function().End()
    .Export()
        .Function("boom")
        .Function("callAnother")
    .End()
    .Code()
        .Function("boom", {params:["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32"], ret:"i32"})
            /* call doStackCheck */.GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).Call(4)
            .Return()
        .End()
        .Function("callAnother", {params:["i32"], ret:"i32"})
            /* call imp:callAnother */.GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).Call(1)
            /* call doStackCheck */.GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).Call(4)
            .Return()
        .End()
        .Function("doStackCheck", {params:["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32"], ret:"i32"})
            /* call dummy */.GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).GetLocal(0).Call(5)
            .Return()
        .End()
        .Function("dummy", {params:["i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32", "i32"], ret:"i32"})
            .GetLocal(0)
            .Return()
        .End()
    .End();

const bin = builder.WebAssembly().get();
const module = new WebAssembly.Module(bin);

let i1;

const imp = {
    boom: () => { throw new Error(`This boom should not get called!`); },
    callAnother: () => { i1.exports["boom"](0xdeadbeef); },
}

const i0 = new WebAssembly.Instance(module, { imp });
i1 = new WebAssembly.Instance(module, { imp: i0.exports });

i1.exports["callAnother"](0xc0defefe);
