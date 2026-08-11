function foo() {
    const s0 = ''.padStart(2049, ()=>{});
    const s1 = s0.padStart(2050);
    (0)[s0];
    s1[0];
}

const s2 = [10, foo].toLocaleString();
const s3 = eval(s2);
s3();
foo();
