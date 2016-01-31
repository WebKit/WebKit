Number.prototype.negate = function ()
{
    "use strict";
    return -this;
};

for (var i = 1; i < 1e4; ++i)
    (0x424242).negate();
