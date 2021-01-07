//@ runDefault

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

shouldThrow(function(){
    var x = 42;
    var a = [1, 2, 3];
    new x(x = function(){ }, ...a);
}, `TypeError: 42 is not a constructor (evaluating 'new x(x = function(){ }, ...a)')`);
