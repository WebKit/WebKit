// This test should not crash.

var arr = [];
arr.x = 0;
arr.y = 0;
delete arr["x"];

for (var i = 0; i < 2; ++i)
    arr.unshift(i);

arr.z = 42;
