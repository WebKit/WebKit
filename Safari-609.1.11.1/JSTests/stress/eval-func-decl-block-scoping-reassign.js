var assert = function (result, expected, message) {
  if (result !== expected) {
    throw new Error('Error in assert. Expected "' + expected + '" but was "' + result + '":' + message );
  }
};

{
  function f() {
    return 'first declaration';
  }
}

eval(
  '{ function f() { return "second declaration"; } }'
);

assert(typeof f, 'function', ' #1');
assert(f(), 'second declaration', ' #2');