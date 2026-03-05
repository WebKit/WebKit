// Shallow rope slice benchmark (regression check)
let s = 'A'.repeat(25000) + 'B'.repeat(25000);
let n = 50000;

for (let i = 0; i < n; i++)
    s.slice(i, i + 1);
