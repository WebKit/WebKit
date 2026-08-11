[
  EvalError, RangeError, ReferenceError,
  SyntaxError, TypeError, URIError,
].forEach((NativeError) => {
  if (NativeError.prototype.hasOwnProperty('toString')) {
    throw new Error(`${NativeError.name}.prototype should not have own property 'toString'`);
  }
  if (NativeError.prototype.toString !== Error.prototype.toString) {
    throw new Error(`${NativeError.name}.prototype.toString should equal Error.prototype.toString`);
  }
});
