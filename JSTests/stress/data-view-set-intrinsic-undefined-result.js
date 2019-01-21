let setInt8 = DataView.prototype.setInt8;

function foo() {
  new bar();
  xyz(setInt8(0, 0));
}

function bar(a) {
  if (a) {
    return;
  }
  if (0 === undefined) {
  }
  a = + String(0);
  foo(0);
}

try {
    foo();
} catch { }
