function getProperties(obj) {
    let proto = Object.getPrototypeOf(obj);
}
function getRandomProperty(obj) {
    let properties = getProperties(obj);
}
var number = 981428;
getRandomProperty(number);
for (var i = 0; i < 100000; ++i) {
    try {
        undef, void false;
    } catch (e) {
    }
}
