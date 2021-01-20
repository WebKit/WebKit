"use strict";

function tail() { }

var obj = {
    get x() { return tail(0); }
};

for (var i = 0; i < 10; ++i)
    obj.x;
