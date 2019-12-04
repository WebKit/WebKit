import Builder from '../Builder.js'
import * as assert from '../assert.js'

{
    const b = new Builder();
    let cont = b
        .Type().End()
        .Function().End()
        .Global()
            .I32(0, "immutable")
            .I64(0, "immutable")
            .F32(0, "immutable")
            .F64(0, "immutable")
            .RefNull("anyref", "immutable")
            .RefNull("funcref", "immutable")
        .End()
        .Export()
            .Global("i32", 0)
            .Global("i64", 1)
            .Global("f32", 2)
            .Global("f64", 3)
            .Global("anyref", 4)
            .Global("funcref", 5)
            .Function("getI32")
            .Function("getI64AsI32")
            .Function("getF32")
            .Function("getF64")
            .Function("getAnyref")
            .Function("getFuncref")
        .End()
        .Code()
            .Function("getI32", { params: [], ret: "i32" }, [])
                .GetGlobal(0)
                .Return()
            .End()
            .Function("getI64AsI32", { params: [], ret: "i32" }, [])
                .GetGlobal(1)
                .I32WrapI64()
                .Return()
            .End()
            .Function("getF32", { params: [], ret: "f32" }, [])
                .GetGlobal(2)
                .Return()
            .End()
            .Function("getF64", { params: [], ret: "f64" }, [])
                .GetGlobal(3)
                .Return()
            .End()
            .Function("getAnyref", { params: [], ret: "anyref" }, [])
                .GetGlobal(4)
                .Return()
            .End()
            .Function("getFuncref", { params: [], ret: "funcref" }, [])
                .GetGlobal(5)
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
            assert.throws(() => binding.value = 42, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getI32(), 0);
        }
        {
            let binding = instance.exports.f32;
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getF32(), 0);
            assert.throws(() => binding.value = 42.5, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getF32(), 0);
        }
        {
            let binding = instance.exports.f64;
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getF64(), 0);
            assert.throws(() => binding.value = 42.5, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
            assert.eq(binding.value, 0);
            assert.eq(instance.exports.getF64(), 0);
        }
        {
            let binding = instance.exports.i64;
            assert.throws(() => binding.value, TypeError, `WebAssembly.Global.prototype.value does not work with i64 type`);
            assert.throws(() => binding.value = 42, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
            assert.eq(instance.exports.getI64AsI32(), 0);
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
                assert.throws(() => binding.value = value, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
                assert.eq(binding.value, null);
            }
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
                assert.throws(() => binding.value = value, TypeError, `WebAssembly.Global.prototype.value attempts to modify immutable global value`);
                assert.eq(binding.value, null);
            }
        }
    }
}
