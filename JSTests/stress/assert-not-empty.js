//@ runDefault("--useObjectAllocationSinking=0")

function foo(o) {
  if (!o) {
    +eval;
  }
  o.x;
};
let i=0;
for (;i<100000;++i) {
  foo(Object);
}
