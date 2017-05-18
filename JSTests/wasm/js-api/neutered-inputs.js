import * as assert from "../assert.js";

let neuteredArray = new Uint8Array(1);
transferArrayBuffer(neuteredArray.buffer);


const testFunction = (func) => {
    assert.throws(() => func(neuteredArray), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
    assert.throws(() => func(neuteredArray.buffer), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
}

const testConstructor = (func) => {
    assert.throws(() => new func(neuteredArray), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
    assert.throws(() => new func(neuteredArray.buffer), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
}

testConstructor(WebAssembly.Module);
testFunction(WebAssembly.compile);
testFunction(WebAssembly.validate);
testFunction(WebAssembly.instantiate);
