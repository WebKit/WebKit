var assert = function (result, expected, message) {
    if (result !== expected) {
        throw new Error('Error in assert. Expected-' + expected + ' but was' + result + ':' + message );
    }
};

var updated;

(function() {
  eval(
    '{\
      function f() {\
        return "first declaration";\
      }\
    }if (true) function f() { return "second declaration"; } else function _f() {}updated = f;'
  );
}());

assert(typeof updated, 'function', "#1");
assert(updated(), 'second declaration', "#2");
