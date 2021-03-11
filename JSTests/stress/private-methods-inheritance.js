class A {
  constructor(a) {}
  #x() {}
}
class B extends A {
  #y() {}
}

let arr = [];
for (let i = 0; i < 10000000; ++i) {
  arr.push(new B(undefined));
}
