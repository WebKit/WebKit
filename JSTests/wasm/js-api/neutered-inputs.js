import * as assert from "../assert.js";

const fail = val => { throw new Error(`Expected promise to fail, instead got ${val}`); };
const catcher = (errType, errMessage) => err => {
    assert.eq(errType, err);
    assert.eq(errMessage, err.message);
};

let neuteredArray = new Uint8Array(1);
transferArrayBuffer(neuteredArray.buffer);

const testAsyncFunction = func => {
    func(neuteredArray).then(fail).catch(catcher(TypeError, "Underlying ArrayBuffer has been detached from the view or out-of-bounds"));
    func(neuteredArray.buffer).then(fail).catch(catcher(TypeError, "Underlying ArrayBuffer has been detached from the view or out-of-bounds"));
};

const testFunction = func => {
    assert.throws(() => func(neuteredArray), TypeError, "Underlying ArrayBuffer has been detached from the view or out-of-bounds");
    assert.throws(() => func(neuteredArray.buffer), TypeError, "Underlying ArrayBuffer has been detached from the view or out-of-bounds");
};

const testConstructor = func => {
    assert.throws(() => new func(neuteredArray), TypeError, "Underlying ArrayBuffer has been detached from the view or out-of-bounds");
    assert.throws(() => new func(neuteredArray.buffer), TypeError, "Underlying ArrayBuffer has been detached from the view or out-of-bounds");
};

testConstructor(WebAssembly.Module);
testAsyncFunction(WebAssembly.compile);
testFunction(WebAssembly.validate);
testAsyncFunction(WebAssembly.instantiate);
