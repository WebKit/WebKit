//@ requireOptions("-e", "let hardness=100") if ! $memoryLimited
//@ requireOptions("-e", "let hardness=20") if $memoryLimited
//@ runDefault

let theCode = `
for (let i=0; i<10000; i++) {
    0 .toLocaleString();
}
`;

for (let i = 0; i < hardness; i++)
    $.agent.start(theCode);
