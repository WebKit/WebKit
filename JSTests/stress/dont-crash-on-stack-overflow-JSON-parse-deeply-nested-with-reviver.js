//@ runDefault("--maxPerThreadStackUsage=5242880")

// This test should gracefully fail with a stack
// overflow error which we can catch.

obj = {}

// create the deeply nested JSON objects
for (let i = 0; i < 4600; ++i) {
 obj = {obj};
}

let identityReviver = (key, value) => {
  // perform no computation and leave values unchanged
  value
}

// Pass in an identity reviver function to JSON.parse. 
// https://tc39.es/ecma262/multipage/structured-data.html#sec-json.parse
// Stack overflow still ocurrs with an identity reviver. 
// The actual compuation of the reviver function is not 
// relevant.
// Still do need to pass in a reviver function because 
// otherwise jsonParseSlow will not be called 
// (see logic in jsonProtoFuncParse of JSONObject.cpp).
let stringified = JSON.stringify(obj);
try {
    let parsed = JSON.parse(stringified, identityReviver);
} catch(e) {
    if (e != "RangeError: Maximum call stack size exceeded.")
        throw e;
}
