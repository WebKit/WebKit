const a0 = [ 2, 0.3 ];
const o = {};
function foo(arg) {
    for (const c of '123456') {
        let b = arg instanceof Array;
        let cond = a0[-b] < 1;
        do {} while (cond);
        o[arg] = undefined;
    }
}
foo([]);
foo('');
foo('');
foo('');
