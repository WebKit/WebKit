var array = [];
var funky = {
    toJSON: [array, -155]
};

array[0] = funky.toJSON;

var exception;
try {
    JSON.stringify(array);
} catch (e) {
    exception = e;
}

if (exception != "TypeError: JSON.stringify cannot serialize cyclic structures.")
    throw "FAILED";

