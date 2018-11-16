//@ if $memoryLimited then skip else runDefault end

function make_contig_arr(sz)
{
    let a = []; 
    while (a.length < sz / 8)
        a.push(3.14); 
    a.length *= 8;
    return a;
}

try {
    let ARRAY_LENGTH = 0x10000000;
    let a = make_contig_arr(ARRAY_LENGTH);
    let b = make_contig_arr(0xff00);
    b.unshift(a.length-0x10000, 0);
    Array.prototype.splice.apply(a, b);
} catch (e) {}
