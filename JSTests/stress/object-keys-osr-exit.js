//@ runDefault("--forceEagerCompilation=1", "--useConcurrentJIT=0")

function foo(x) {
  if (x) {
    return;
  }
  let obj = {
    a: 0,
    b: 0
  };
  foo(1);
  let keys = Object.keys(obj);
  foo();
  keys.length
}

try {
  foo();
} catch(e) {
  if (e != "RangeError: Maximum call stack size exceeded.")
  throw "FAILED";
}
