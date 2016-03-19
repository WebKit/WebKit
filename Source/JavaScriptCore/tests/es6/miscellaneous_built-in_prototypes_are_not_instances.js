function test() {

try {
  RegExp.prototype.source; return false;
} catch(e) {}
try {
  Date.prototype.valueOf(); return false;
} catch(e) {}

if (![Error, EvalError, RangeError, ReferenceError, SyntaxError, TypeError, URIError].every(function (E) {
    return Object.prototype.toString.call(E.prototype) === '[object Object]';
})) {
  return false;
}

return true;
      
}

if (!test())
    throw new Error("Test failed");

