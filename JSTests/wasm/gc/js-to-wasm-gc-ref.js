import * as assert from "../assert.js";
import { instantiate } from "./wast-wrapper.js";

function throwsType(fn) {
    try {
        fn();
    } catch (e) {
        if (e instanceof TypeError)
            return;
        throw e;
    }
    throw new Error("did not throw");
}

let m = await instantiate(`
  (module
    (type $a (sub (array (mut i8))))
    (type $b (sub $a (array (mut i8))))
    (type $s (struct (field (mut i32))))
    (func (export "make") (param i32) (result (ref $a))
      (array.new $a (i32.const 7) (local.get 0)))
    (func (export "makeSub") (param i32) (result (ref $b))
      (array.new $b (i32.const 3) (local.get 0)))
    (func (export "makeStruct") (result (ref $s))
      (struct.new $s (i32.const 5)))
    (func (export "set") (param (ref $a) i32 i32)
      (array.set $a (local.get 0) (local.get 1) (local.get 2)))
    (func (export "get") (param (ref $a) i32) (result i32)
      (array.get_u $a (local.get 0) (local.get 1)))
    (func (export "isNull") (param (ref null $a)) (result i32)
      (ref.is_null (local.get 0)))
    (func (export "structGet") (param (ref $s)) (result i32)
      (struct.get $s 0 (local.get 0))))
`);

let array = m.exports.make(8);
let sub = m.exports.makeSub(8);
for (let i = 0; i < 10000; i++)
    m.exports.set(array, i & 7, i & 255);
m.exports.set(array, 1, 9);
assert.eq(m.exports.get(array, 1), 9);
m.exports.set(sub, 2, 4);
assert.eq(m.exports.get(sub, 2), 4);
assert.eq(m.exports.isNull(null), 1);
assert.eq(m.exports.isNull(array), 0);
assert.eq(m.exports.structGet(m.exports.makeStruct()), 5);

throwsType(() => m.exports.set(null, 0, 1));
throwsType(() => m.exports.set({}, 0, 1));
throwsType(() => m.exports.set(m.exports.makeStruct(), 0, 1));
throwsType(() => m.exports.structGet(array));
