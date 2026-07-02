let x = {
  __proto__: Object,
};
function foo() {
  Object.defineProperty(Object, 0, {});
}
let o = {
  toString: foo
};
delete {}[[...[o]]];
