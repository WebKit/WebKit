let code = `
function* gen(a) {
  a.b;
  a.b;
  a.b;
  a.b;
  a.b;
  yield 1;
  ` + " ".repeat(9000000) + `a.b;
}
let it = gen({});
it.next();
it.next();
`;

eval(code);

