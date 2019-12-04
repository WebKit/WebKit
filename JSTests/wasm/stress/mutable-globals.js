import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    let cont = b
        .Type().End()
        .Function().End()
        .Global()
            .I32(0, "mutable")
            .I64(0, "mutable")
            .F32(0, "mutable")
            .F64(0, "mutable")
            .RefNull("anyref", "mutable")
            .RefNull("funcref", "mutable")
        .End()
        .Export()
            .Global("i32", 0)
            .Global("i64", 1)
            .Global("f32", 2)
            .Global("f64", 3)
            .Global("anyref", 4)
            .Global("funcref", 5)
            .Function("getI32")
            .Function("setI32")
            .Function("getI64AsI32")
            .Function("setI32AsI64")
            .Function("getF32")
            .Function("setF32")
            .Function("getF64")
            .Function("setF64")
            .Function("getAnyref")
            .Function("setAnyref")
            .Function("getFuncref")
            .Function("setFuncref")
        .End()
        .Code()
            .Function("getI32", { params: [], ret: "i32" }, [])
                .GetGlobal(0)
                .Return()
            .End()
            .Function("setI32", { params: ["i32"], ret: "void" }, [])
                .GetLocal(0)
                .SetGlobal(0)
                .Return()
            .End()
            .Function("getI64AsI32", { params: [], ret: "i32" }, [])
                .GetGlobal(1)
                .I32WrapI64()
                .Return()
            .End()
            .Function("setI32AsI64", { params: ["i32"], ret: "void" }, [])
                .GetLocal(0)
                .I64ExtendSI32()
                .SetGlobal(1)
                .Return()
            .End()
            .Function("getF32", { params: [], ret: "f32" }, [])
                .GetGlobal(2)
                .Return()
            .End()
            .Function("setF32", { params: ["f32"], ret: "void" }, [])
                .GetLocal(0)
                .SetGlobal(2)
                .Return()
            .End()
            .Function("getF64", { params: [], ret: "f64" }, [])
                .GetGlobal(3)
                .Return()
            .End()
            .Function("setF64", { params: ["f64"], ret: "void" }, [])
                .GetLocal(0)
                .SetGlobal(3)
                .Return()
            .End()
            .Function("getAnyref", { params: [], ret: "anyref" }, [])
                .GetGlobal(4)
                .Return()
            .End()
            .Function("setAnyref", { params: ["anyref"], ret: "void" }, [])
                .GetLocal(0)
                .SetGlobal(4)
                .Return()
            .End()
            .Function("getFuncref", { params: [], ret: "funcref" }, [])
                .GetGlobal(5)
                .Return()
            .End()
            .Function("setFuncref", { params: ["funcref"], ret: "void" }, [])
                .GetLocal(0)
                .SetGlobal(5)
                .Return()
            .End()
        .End()

    const bin = b.WebAssembly().get();
    const module = new WebAssembly.Module(bin);
    const instance = new WebAssembly.Instance(module);

    for (var i = 0; i < 1e3; ++i) {
        {
            let binding = instance.exports.i32;
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getI32(), 0);
            binding.value = 42;
            assert.eq(binding.value, 42);
            assert.eq(instance.exports.getI32(), 42);
            instance.exports.setI32(20);
            assert.eq(instance.exports.getI32(), 20);
            assert.eq(binding.value, 20);
            binding.value = 0;
        }
        {
            let binding = instance.exports.f32;
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getF32(), 0);
            binding.value = 42.5;
            assert.eq(binding.value, 42.5);
            assert.eq(instance.exports.getF32(), 42.5);
            instance.exports.setF32(20.5);
            assert.eq(instance.exports.getF32(), 20.5);
            assert.eq(binding.value, 20.5);
            binding.value = 0;
        }
        {
            let binding = instance.exports.f64;
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getF64(), 0);
            binding.value = 42.5;
            assert.eq(binding.value, 42.5);
            assert.eq(instance.exports.getF64(), 42.5);
            instance.exports.setF64(20.5);
            assert.eq(instance.exports.getF64(), 20.5);
            assert.eq(binding.value, 20.5);
            binding.value = 0;
        }
        {
            let binding = instance.exports.i64;
            assert.throws(() => binding.value, TypeError, `WebAssembly.Global.prototype.value does not work with i64 type`);
            assert.throws(() => binding.value = 42, TypeError, `WebAssembly.Global.prototype.value does not work with i64 type`);
            assert.eq(instance.exports.getI64AsI32(), 0);
            instance.exports.setI32AsI64(20);
            assert.eq(instance.exports.getI64AsI32(), 20);
            instance.exports.setI32AsI64(0);
        }
        {
            let binding = instance.exports.anyref;
            assert.eq(binding.value, null);
            assert.eq(instance.exports.getAnyref(), null);

            let list = [
                undefined,
                null,
                0,
                5.4,
                "Hey",
                {},
                function () { },
                Symbol("Cocoa"),
                false,
            ];
            for (let value of list) {
                binding.value = value;
                assert.eq(binding.value, value);
                assert.eq(instance.exports.getAnyref(), value);
                instance.exports.setAnyref(null);
                assert.eq(instance.exports.getAnyref(), null);
                instance.exports.setAnyref(value);
                assert.eq(instance.exports.getAnyref(), value);
            }
            binding.value = null;
        }
        {
            let binding = instance.exports.funcref;
            assert.eq(binding.value, null);
            assert.eq(instance.exports.getFuncref(), null);

            let list = [
                undefined,
                0,
                5.4,
                "Hey",
                {},
                function () { },
                Symbol("Cocoa"),
                false,
            ];
            for (let value of list) {
                assert.throws(() => binding.value = value, WebAssembly.RuntimeError, `Funcref must be an exported wasm function (evaluating 'binding.value = value')`);
                assert.eq(binding.value, null);
                assert.eq(instance.exports.getFuncref(), null);
                instance.exports.setFuncref(null);
                assert.eq(instance.exports.getFuncref(), null);
                assert.throws(() => instance.exports.setFuncref(value), WebAssembly.RuntimeError, `Funcref must be an exported wasm function (evaluating 'func(...args)')`);
                assert.eq(instance.exports.getFuncref(), null);
            }
            binding.value = instance.exports.setFuncref;
            assert.eq(binding.value, instance.exports.setFuncref);
            assert.eq(instance.exports.getFuncref(), instance.exports.setFuncref);
            binding.value = null;
            instance.exports.setFuncref(instance.exports.setFuncref);
            assert.eq(binding.value, instance.exports.setFuncref);
            assert.eq(instance.exports.getFuncref(), instance.exports.setFuncref);
            binding.value = null;
        }
    }
}
