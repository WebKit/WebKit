let lexicalVariable = true;

function testNativeScope() {
    var p = new Promise(function(resolve, reject) {
        debugger;
    })

    return p;
}

