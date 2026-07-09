var keys = ['utm_source', 'utm_medium', 'campaign_id', 'session_token', 'user_locale', 'page_number', 'sort_order', 'filter_state'];
var vals = [];
for (var j = 0; j < 16; ++j)
    vals.push('value_' + (j * 7919) + '_abcdefgh');

function build(i) {
    var parts = [];
    for (var j = 0; j < 8; ++j)
        parts.push(`${keys[j]}=${vals[(i + j) & 15]}`);
    return parts.join('&');
}
noInline(build);

var sum = 0;
for (var i = 0; i < 200000; ++i)
    sum += build(i).length;

if (sum !== 52400000)
    throw new Error("bad sum: " + sum);
