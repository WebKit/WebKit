const x = Object.getOwnPropertyDescriptors(new Uint8Array(10));
Object.defineProperty(x, 9, {get: foo});

function foo() {
  Object.create(()=>{}, x);
}

var hadRangeError = false;
try {
    foo();
} catch (e) {
    if (e.name != "RangeError")
        throw "Wrong exception";
    hadRangeError = true;
}
if (!hadRangeError)
    throw "Should have raised an exception";
