//@ runDefault("--useObjectAllocationSinking=0")

function foo(o) {
  if (!o) {
    +eval;
  }
  o.x;
};
let i=0;
for (;i<testLoopCount;++i) {
  foo(Object);
}
