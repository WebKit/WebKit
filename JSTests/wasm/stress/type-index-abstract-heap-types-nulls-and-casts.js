//@ slow!
// https://bugs.webkit.org/show_bug.cgi?id=247454
import * as assert from "../assert.js";
import { compile, instantiate } from "../gc/wast-wrapper.js";

function testAbstractNullsAndCasts() {
    const m = instantiate(`
      (module
        (type $S (struct (field i32)))
        (type $A (array i32))
        (func (export "nullFuncref") (result funcref) (ref.null func))
        (func (export "nullExternref") (result externref) (ref.null extern))
        (func (export "nullAnyref") (result anyref) (ref.null any))
        (func (export "nullEqref") (result eqref) (ref.null eq))
        (func (export "nullI31ref") (result i31ref) (ref.null i31))
        (func (export "nullStructref") (result structref) (ref.null struct))
        (func (export "nullArrayref") (result arrayref) (ref.null array))
        (func (export "nullNone") (result nullref) (ref.null none))
        (func (export "nullNofunc") (result nullfuncref) (ref.null nofunc))
        (func (export "nullNoextern") (result nullexternref) (ref.null noextern))

        (func (export "castI31ToAny") (param i32) (result anyref)
          (ref.cast anyref (ref.i31 (local.get 0))))
        (func (export "testI31IsEq") (param i32) (result i32)
          (ref.test eqref (ref.i31 (local.get 0))))
        (func (export "testStructIsAny") (result i32)
          (ref.test anyref (struct.new $S (i32.const 1))))
        (func (export "testArrayIsStruct") (result i32)
          (ref.test structref (array.new_default $A (i32.const 1))))
        (func (export "castStructToStructref") (result structref)
          (ref.cast structref (struct.new $S (i32.const 7))))
        (func (export "castArrayToArrayref") (result arrayref)
          (ref.cast arrayref (array.new_default $A (i32.const 2))))
        (func (export "castFailAnyToStruct") (param anyref) (result (ref null $S))
          (ref.cast (ref null $S) (local.get 0)))
      )
    `).exports;

    assert.eq(m.nullFuncref(), null);
    assert.eq(m.nullExternref(), null);
    assert.eq(m.nullAnyref(), null);
    assert.eq(m.nullEqref(), null);
    assert.eq(m.nullI31ref(), null);
    assert.eq(m.nullStructref(), null);
    assert.eq(m.nullArrayref(), null);
    assert.eq(m.nullNone(), null);
    assert.eq(m.nullNofunc(), null);
    assert.eq(m.nullNoextern(), null);

    assert.eq(m.testI31IsEq(42), 1);
    assert.eq(m.testStructIsAny(), 1);
    assert.eq(m.testArrayIsStruct(), 0);
    m.castI31ToAny(99);
    m.castStructToStructref();
    m.castArrayToArrayref();

    assert.throws(
        () => m.castFailAnyToStruct(42),
        WebAssembly.RuntimeError,
        "cast"
    );
}

for (let i = 0; i < wasmTestLoopCount; ++i)
    testAbstractNullsAndCasts();
