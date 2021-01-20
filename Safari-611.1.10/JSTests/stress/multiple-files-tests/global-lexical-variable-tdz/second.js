
for (var i = 0; i < 1000; i++)
    shouldThrowTDZ(foo);

let lexicalVariableNotYetDefined = 100;
assert(foo() === lexicalVariableNotYetDefined);


for (var i = 0; i < 1000; i++)
    shouldThrowTDZ(bar);
let lexicalVariableNotYetDefinedSecond = 200;
assert(bar() === 300);
