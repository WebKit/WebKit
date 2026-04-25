Boolean.prototype.negate = function ()
{
    "use strict";
    return !this;
};
noInline(Boolean.prototype.negate);

for (var i = 0; i < 1e4; ++i)
    (i % 4 === 0).negate();
Boolean.prototype.negate.call(true);

for (var i = 0; i < 1e4; ++i)
    Boolean.prototype.negate.call(i);

Boolean.prototype.negate2 = function ()
{
    "use strict";
    return !this;
};

for (var i = 0; i < 1e4; ++i)
    (true).negate2();
