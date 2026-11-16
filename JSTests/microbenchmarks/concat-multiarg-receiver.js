var base = [];
for (var j = 0; j < 8; j++)
    base.push({ name: 'base' + j });
var user = [];
for (var j = 0; j < 8; j++)
    user.push({ name: 'user' + j });
var extra = [];
for (var j = 0; j < 2; j++)
    extra.push({ name: 'extra' + j });

var result = 0;
for (var i = 0; i < 2e6; i++) {
    var r = base.concat(user, extra);
    result += r.length;
}

if (result !== 36e6)
    throw new Error("bad result: " + result);
