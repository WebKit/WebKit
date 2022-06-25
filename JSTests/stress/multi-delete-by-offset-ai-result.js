//@ runDefault("--validateAbstractInterpreterState=1")

function foo(obj) {
  return delete obj['x'];
}
noInline(foo);

let o = {};

for (let i = 0; i < 10000; ++i) {
  Object.defineProperty(o, 'x', {});
  foo({});
  foo({x:1});
}

