let s0 = '000'.toLocaleUpperCase();
let s1 = s0.slice(1);
s1[s0] = undefined;
s1.toWellFormed();
