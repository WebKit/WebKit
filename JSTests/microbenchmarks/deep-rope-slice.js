let n = 10000;
let s = '';
for (let i = 0; i < n; i++)
    s += 'A';

for (let i = 0; i < n; i++)
    s.slice(i, i + 1);
