//@ skip if ["arm", "mips"].include?($architecture)
//@ runDefault

let theCode = `
for (let i=0; i<10000; i++) {
    0 .toLocaleString();
}
`;

for (let i = 0; i < 100; i++)
    $.agent.start(theCode);
