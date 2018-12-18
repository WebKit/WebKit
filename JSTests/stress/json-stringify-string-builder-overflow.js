//@ slow
//@ skip if $architecture != "arm64" and $architecture != "x86-64"

var exception;
try {
    var str = JSON.stringify({
        'a1': {
            'a2': {
                'a3': {
                    'a4': {
                        'a5': {
                            'a6': 'AAAAAAAAAA'
                        }
                    }
                }
            }
        }
    }, function (key, value) {
        var val = {
            'A': true,
        };
        return val;
    }, 1);
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
