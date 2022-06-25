Symbol.prototype.identity = function ()
{
    "use strict";
    return this;
};
noInline(Symbol.prototype.identity);

Symbol.prototype.identity2 = function ()
{
    "use strict";
    return this;
};

for (var i = 1; i < 1e4; ++i)
    Symbol.prototype.identity.call(Symbol(i));

for (var i = 1; i < 1e4; ++i)
    Symbol.prototype.identity2.call(Symbol(i));
