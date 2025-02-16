var length = 1024;
var array = new Array(1024);
array.fill(99);
ensureArrayStorage(array);

var result;
for (let i = 0; i < 1e5; i++) {
    result = array.with(i % 1024, i);
    result = array.with((i + 1) % 1024, i + 1);
}
