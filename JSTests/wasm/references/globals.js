import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

// trivial
function Pelmen(calories) {
  this.calories = calories;
}
const calories = 100;

function testGlobalConstructorForExternref() {
  {
      let global = new WebAssembly.Global({ value: "externref", mutable: true });
      assert.eq(global.value, undefined);

      global.value = new Pelmen(calories);
      assert.eq(global.value.calories, calories);
  }

  {
      let global = new WebAssembly.Global({ value: "externref", mutable: true }, new Pelmen(calories));
      assert.eq(global.value.calories, calories);
  }
}

async function testGlobalConstructorForFuncref() {
  const instance = await instantiate(`(module (func (export "foo")))`, {}, {reference_types: true});

  {
      let global = new WebAssembly.Global({ value: "anyfunc", mutable: true });
      assert.eq(global.value, null);

      global.value = instance.exports.foo;
      assert.eq(global.value, instance.exports.foo);
  }

  {
      let global = new WebAssembly.Global({ value: "anyfunc", mutable: true }, instance.exports.foo);
      assert.eq(global.value, instance.exports.foo);
      assert.throws(() => global.value = new Pelmen(calories), WebAssembly.RuntimeError, "Funcref must be an exported wasm function (evaluating 'global.value = new Pelmen(calories)')");
  }

  assert.throws(() => new WebAssembly.Global({ value: "anyfunc", mutable: true }, new Pelmen(calories)), WebAssembly.RuntimeError, "Funcref must be an exported wasm function (evaluating 'new WebAssembly.Global({ value: \"anyfunc\", mutable: true }, new Pelmen(calories))')");
}

testGlobalConstructorForExternref();
assert.asyncTest(testGlobalConstructorForFuncref());
