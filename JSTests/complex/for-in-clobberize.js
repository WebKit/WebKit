Array.prototype.__proto__ = {};
let a = [];
for (let i=0; i<100; i++) {
  a.unshift(undefined);
  for (let x in a);
}
