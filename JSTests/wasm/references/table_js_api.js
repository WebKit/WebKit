import * as assert from '../assert.js';
import { instantiate } from "../wabt-wrapper.js";

// trivial
function Pelmen(calories) {
  this.calories = calories;
}
const calories = 100;

function testTableGrowForExternrefTables() {
  const table = new WebAssembly.Table({initial:1, maximum:10, element: "externref"});
  table.grow(3, new Pelmen(calories));
  assert.eq(table.get(1).calories, calories);
  assert.eq(table.get(2).calories, calories);
  assert.eq(table.get(3).calories, calories);

  table.grow(3);
  assert.eq(table.length, 7);
  assert.eq(table.get(4), undefined);
  assert.eq(table.get(5), undefined);
  assert.eq(table.get(6), undefined);
}

async function testTableGrowForFuncrefTables() {
  const table = new WebAssembly.Table({initial:1, maximum:10, element: "funcref"});
  const calories = 100;
  assert.throws(() => table.grow(3, new Pelmen(calories)), Error, "WebAssembly.Table.prototype.grow expects the second argument to be null or an instance of WebAssembly.Function");

  table.grow(3, null);
  assert.eq(table.get(1), null);
  assert.eq(table.get(2), null);
  assert.eq(table.get(3), null);

  const instance = await instantiate(`(module (func (export "foo")))`, {}, {reference_types: true});
  table.grow(3, instance.exports.foo);
  assert.eq(table.get(4), instance.exports.foo);
  assert.eq(table.get(5), instance.exports.foo);
  assert.eq(table.get(6), instance.exports.foo);
}

function testTableConstructorForExternrefTables() {
  const table = new WebAssembly.Table({initial:3, maximum:10, element: "externref"}, new Pelmen(calories));
  assert.eq(table.get(0).calories, calories);
  assert.eq(table.get(1).calories, calories);
  assert.eq(table.get(2).calories, calories);
  assert.throws(() => table.get(3), RangeError, "WebAssembly.Table.prototype.get expects an integer less than the length of the table");
}

async function testTableConstructorForFuncrefTables() {
  const instance = await instantiate(`(module (func (export "foo")))`, {}, {reference_types: true});
  const table = new WebAssembly.Table({initial:3, maximum:10, element: "funcref"}, instance.exports.foo);
  assert.eq(table.get(0), instance.exports.foo);
  assert.eq(table.get(1), instance.exports.foo);
  assert.eq(table.get(2), instance.exports.foo);

  assert.throws(() => new WebAssembly.Table({initial:3, maximum:10, element: "funcref"}, {}), TypeError, "WebAssembly.Table.prototype.constructor expects the second argument to be null or an instance of WebAssembly.Function");
}

function testTableSetForExternrefTables() {
  const table = new WebAssembly.Table({initial:3, maximum:10, element: "externref"}, new Pelmen(calories));
  assert.eq(table.get(0).calories, calories);

  table.set(0);
  assert.eq(table.get(0), undefined);

  assert.throws(() => table.set(4), RangeError, "WebAssembly.Table.prototype.set expects an integer less than the length of the table");
  assert.throws(() => table.set(100500), RangeError, "WebAssembly.Table.prototype.set expects an integer less than the length of the table");
}

async function testTableSetForFuncrefTables() {
  const instance = await instantiate(`(module (func (export "foo")))`, {}, {reference_types: true});
  const table = new WebAssembly.Table({initial:3, maximum:10, element: "funcref"});
  assert.eq(table.get(0), null);

  table.set(0, instance.exports.foo);
  assert.eq(table.get(0), instance.exports.foo);

  assert.throws(() => table.set(0, {}), TypeError, "WebAssembly.Table.prototype.set expects the second argument to be null or an instance of WebAssembly.Function");
}

testTableGrowForExternrefTables();
assert.asyncTest(testTableGrowForFuncrefTables());

testTableConstructorForExternrefTables();
assert.asyncTest(testTableConstructorForFuncrefTables());

testTableSetForExternrefTables();
assert.asyncTest(testTableSetForFuncrefTables());
