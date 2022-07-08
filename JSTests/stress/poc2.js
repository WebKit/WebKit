const limit = 3;
let depth;
let a = {
  get length() {
    new foo();
  }
};

function foo() {
  if (depth >= limit) {
    OSRExit();
    return;
  }
  ++depth;
  foo.apply(undefined, a);
  gc();
}

for (let i = 0; i < 100000; ++i) {
  depth = 0;
  foo.apply(undefined, a);
}
