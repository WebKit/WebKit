//@ runDefault
function f(a){
    try{
        let b=((a=b),{t(){b}})
    }catch{
    }
    a._
}

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

shouldThrow(f, "TypeError: undefined is not an object (evaluating 'a._')")
