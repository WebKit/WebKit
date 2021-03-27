//@ runDefault("--validateAbstractInterpreterState=1", "--validateAbstractInterpreterStateProbability=1", "--forceEagerCompilation=true")
class A {
  constructor(a) {}
  #x() {}
}
class B extends A {
  #y() {}
}

let arr = [];
for (let i = 0; i < 1e5; ++i) {
  arr.push(new B(undefined));
}
