globalThis.a = 0;
function f1(v)
{
    let x = 40;
    function f2() {
        x;
        let y = 41;
        function f3() {
            let z = 44;
            function f4() {
                z;
                if (v)
                    return a;
                return 1;
            }
            return f4();
        }
        return f3();
    }
    return f2();
}
var N = 2;
for (var i = 0; i < N; ++i) {
    $.evalScript(`let i${i} = 42`);
}
if (f1(false) !== 1) {
    throw new Error('first');
}
$.evalScript(`let a = 42`);
if (f1(true) !== 42)
    throw new Error('second');
