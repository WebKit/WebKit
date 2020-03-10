//@ skip if !$memoryLimited
//@ runDefault

// This test is the same as numberingSystemsForLocale-cached-strings-should-be-immortal-and-safe-for-concurrent-access.js but generating only 20 threads for memory limited machines
let theCode = `
for (let i=0; i<10000; i++) {
    0 .toLocaleString();
}
`;

for (let i = 0; i < 25; i++)
    $.agent.start(theCode);
