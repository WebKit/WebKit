// to be run with useLLInt = false
var o = {};

function getSomeProperty(){
    return o.someProperty;
}

var count = 0;
function test(){
    count++;
    if (count == 3) {
        Object.defineProperty(this, 'someProperty', { value : "okay" });
        return getSomeProperty();
    }
    return "okay";
}

o.__defineGetter__('someProperty', test)

for (var i = 0; i < 4; i++) {
    if (getSomeProperty() != "okay")
        throw ("Error: " + i);
}
