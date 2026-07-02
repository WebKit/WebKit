String.prototype.prefix = function (string)
{
    "use strict";
    // ToThis should be converted to Identity.
    return string + this;
};
noInline(String.prototype.prefix);

String.prototype.first = function (string)
{
    return this.second(string);
};

String.prototype.second = function (string)
{
    // Duplicate ToThis(in first) should be converted to Identity.
    return this + string;
};
noInline(String.prototype.first);


for (var i = 0; i < 1e4; ++i)
    String(i).prefix("Hello");


for (var i = 0; i < 1e4; ++i)
    String(i).first("Hello");
