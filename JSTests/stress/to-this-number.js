Number.prototype.negate = function ()
{
    "use strict";
    return -this;
};
noInline(Number.prototype.negate);

for (var i = 0; i < 1e4; ++i)
    (i % 3 === 0 ? -i : i).negate();

for (var i = 0; i < 1e4; ++i)
    ((i % 3 === 0 ? -i : i) * 0.2).negate();

for (var i = 0; i < 1e4; ++i)
    ((i % 3 === 0 ? -i : i) * 1000000).negate();

Number.prototype.negate.call(-20000);

for (var i = 0; i < 1e4; ++i)
    Number.prototype.negate.call(i % 2 === 0);
