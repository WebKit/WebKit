"use strict";

function Foo() {}

function second(next, cp) {
  return 100;
}

function first(next, cp) {
  return cp < 60 ? new Foo() : next(cp);
}

function createClosure(next, strategy) {
  return function closure(cp) {
    return strategy(next, cp);
  };
}

var tmp = createClosure(null, second);
var bar = createClosure(tmp, first);

noInline(bar);

for (var i=0; i<50000; i++) {
  bar(32);
  bar(32);
  bar(32);
  bar(100);
}
