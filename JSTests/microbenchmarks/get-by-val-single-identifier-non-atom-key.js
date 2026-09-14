function header(name) {
    var lc = name.toLowerCase();
    switch (lc) {
    case 'referer':
    case 'referrer':
        return this.headers.referrer || this.headers.referer;
    default:
        return this.headers[lc];
    }
}

var req = { headers: { 'user-agent': 'curl/8', 'x-request-id': 'abc', 'host': 'example.com', 'accept': '*/*' }, get: header };

var n = 0;
for (var i = 0; i < 1e6; ++i) {
    if (req.get('user-agent') === 'curl/8')
        n++;
    if (req.get('X-Request-Id') === 'abc')
        n++;
}
if (n !== 2e6)
    throw new Error("bad " + n);
