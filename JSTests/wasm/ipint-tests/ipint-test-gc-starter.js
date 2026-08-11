import * as assert from "../assert.js"

function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

/*
 */

function test() {
    const instance = new WebAssembly.Instance(module(""));
    const { main } = instance.exports
    assert.eq(main(), 0)
}

test()

