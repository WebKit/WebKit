// This shouldn't crash when running with ASAN.
let arr = [];
for (var i = 0; i < testLoopCount; i++)
    arr[i] = [];

arr.lastIndexOf(new Object(), {
    valueOf: function () {
        arr.length = 0;
        return 0;
    }
});
