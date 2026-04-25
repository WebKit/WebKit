let nArgs = 50;

let func = 'function foo(';
for (let i = 0; i < nArgs - 1; i++)
    func += `arg${i}, `;
func += `args${nArgs - 1}) {\n`;
func += '   return ';
for (let i = 0; i < nArgs - 1; i++)
    func += `arg${i} + `;
func += `args${nArgs - 1};\n`;
func += '}\n';
func += 'noInline(foo);\n';

let caller = `
function bar() {
   let sum = 0;
   for (let i = 0; i < 10; i++)
      sum += foo(
`;
for (let i = 0; i < nArgs - 1; i++)
    caller += `${i}, `;
caller += `${nArgs - 1});\n`;
caller += `
   return sum;
}
`;

let main = `
let sum = 0;
for (let iters = 0; iters < testLoopCount; iters++)
   sum += bar();
sum;
`

let result = eval(func + caller + main);
let expected = (nArgs - 1) * (nArgs / 2) * 10 * testLoopCount
if (result != expected)
    throw `got ${result} but expected ${expected}`;
