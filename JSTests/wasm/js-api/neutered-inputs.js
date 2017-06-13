import * as assert from "../assert.js";

const fail = val => { throw new Error(`Expected promise to fail, instead got ${val}`); };
const catcher = (errType, errMessage) => err => {
    assert.eq(errType, err);
    assert.eq(errMessage, err.message);
};

let neuteredArray = new Uint8Array(1);
transferArrayBuffer(neuteredArray.buffer);

const testAsyncFunction = func => {
    func(neuteredArray).then(fail).catch(catcher(TypeError, "underlying TypedArray has been detatched from the ArrayBuffer"));
    func(neuteredArray.buffer).then(fail).catch(catcher(TypeError, "underlying TypedArray has been detatched from the ArrayBuffer"));
};

const testFunction = func => {
    assert.throws(() => func(neuteredArray), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
    assert.throws(() => func(neuteredArray.buffer), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
};

const testConstructor = func => {
    assert.throws(() => new func(neuteredArray), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
    assert.throws(() => new func(neuteredArray.buffer), TypeError, "underlying TypedArray has been detatched from the ArrayBuffer");
};

testConstructor(WebAssembly.Module);
testAsyncFunction(WebAssembly.compile);
testFunction(WebAssembly.validate);
testAsyncFunction(WebAssembly.instantiate);
