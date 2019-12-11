//@ requireOptions('--thresholdForJITAfterWarmUp=10', '--useConcurrentJIT=0')

function foo(array) {
    let length = array.length;
    if (length == 1) {
        return array;
    } else {
        for (let i = 1; i < length; ++i) {
            let value = foo(array.slice(0, i));
            foo(array.slice(i));
            value.length;
            foo(array.slice(i));
        }
        return function() {};
    }
}
var array = ['a', 'b', 'c', 'd', 'e'];
foo(array);
