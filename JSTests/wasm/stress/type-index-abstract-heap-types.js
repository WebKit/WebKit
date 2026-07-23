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

function testSubtypeValidation() {
    compile(`
      (module
        (type $S (struct))
        (func (param (ref null $S)) (result)
          (drop (local.get 0)))
        (func (param anyref) (result)
          (call 0 (ref.cast (ref null $S) (local.get 0)))))
    `);

    assert.throws(
        () => compile(`
          (module
            (func (param funcref) (result)
              (drop (ref.cast externref (local.get 0)))))
        `),
        WebAssembly.CompileError,
        "ref.cast"
    );

    assert.throws(
        () => compile(`
          (module
            (func (param externref) (result anyref)
              (ref.cast anyref (local.get 0))))
        `),
        WebAssembly.CompileError,
        "ref.cast"
    );

    compile(`
      (module
        (func (param i31ref) (result anyref)
          (ref.cast anyref (local.get 0))))
    `);

    compile(`
      (module
        (func (param structref) (result eqref)
          (ref.cast eqref (local.get 0))))
    `);
}

function testGlobalsAndTables() {
    const m = instantiate(`
      (module
        (global $g (mut anyref) (ref.null any))
        (global $gf (export "gf") funcref (ref.null func))
        (table $t 2 anyref)
        (func (export "setGlobal") (param anyref)
          (global.set $g (local.get 0)))
        (func (export "getGlobal") (result anyref)
          (global.get $g))
        (func (export "setTable") (param i32 anyref)
          (table.set $t (local.get 0) (local.get 1)))
        (func (export "getTable") (param i32) (result anyref)
          (table.get $t (local.get 0)))
        (func (export "nullInTable") (result i32)
          (table.set $t (i32.const 0) (ref.null none))
          (ref.is_null (table.get $t (i32.const 0))))
      )
    `).exports;

    assert.eq(m.gf.value, null);
    m.setGlobal(null);
    assert.eq(m.getGlobal(), null);
    m.setTable(1, "hello");
    assert.eq(m.getTable(1), "hello");
    assert.eq(m.nullInTable(), 1);
}

function testConcreteVsAbstract() {
    const m = instantiate(`
      (module
        (type $S (sub (struct (field i32))))
        (type $T (sub $S (struct (field i32) (field i64))))
        (func (export "asAny") (result anyref)
          (struct.new $T (i32.const 1) (i64.const 2)))
        (func (export "asStruct") (result structref)
          (struct.new $S (i32.const 3)))
        (func (export "castToS") (param anyref) (result (ref null $S))
          (ref.cast (ref null $S) (local.get 0)))
        (func (export "testIsS") (param anyref) (result i32)
          (ref.test (ref null $S) (local.get 0)))
      )
    `).exports;

    const v = m.asAny();
    assert.eq(m.testIsS(v), 1);
    m.castToS(v);
    m.castToS(m.asStruct());
    assert.eq(m.testIsS(null), 1);
    assert.eq(m.testIsS(42), 0);
}

for (let i = 0; i < wasmTestLoopCount; ++i) {
    testAbstractNullsAndCasts();
    testSubtypeValidation();
    testGlobalsAndTables();
    testConcreteVsAbstract();
}
