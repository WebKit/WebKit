function module(bytes, valid = true) {
  let buffer = new ArrayBuffer(bytes.length);
  let view = new Uint8Array(buffer);
  for (let i = 0; i < bytes.length; ++i) {
    view[i] = bytes.charCodeAt(i);
  }
  return new WebAssembly.Module(buffer);
}

/*
 *  (module
 *    (type (func))
 *    (global funcref (ref.func 0))
 *    (func (type 0)))
 */
new WebAssembly.Instance(module("\x00\x61\x73\x6d\x01\x00\x00\x00\x01\x04\x01\x60\x00\x00\x03\x02\x01\x00\x06\x06\x01\x70\x00\xd2\x00\x0b\x0a\x04\x01\x02\x00\x0b"));
