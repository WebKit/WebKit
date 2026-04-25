if (this.document === undefined) {
      importScripts("/resources/testharness.js");
}

// Four following tests are taken from Chromium fetch tests, see body-mixin.js file
test(t => {
    var req = new Request('/');
    assert_false(req.bodyUsed);
    req.text();
    assert_false(req.bodyUsed);
}, 'BodyUsedShouldNotBeSetForNullBody');

test(t => {
    var req = new Request('/', {method: 'POST', body: ''});
    assert_false(req.bodyUsed);
    req.text();
    assert_true(req.bodyUsed);
}, 'BodyUsedShouldBeSetForEmptyBody');

test(t => {
    var res = new Response('');
    assert_false(res.bodyUsed);
    var reader = res.body.getReader();
    assert_false(res.bodyUsed);
    reader.read();
    assert_true(res.bodyUsed);
}, 'BodyUsedShouldBeSetWhenRead');

test(t => {
    var res = new Response('');
    assert_false(res.bodyUsed);
    var reader = res.body.getReader();
    assert_false(res.bodyUsed);
    reader.cancel();
    assert_true(res.bodyUsed);
}, 'BodyUsedShouldBeSetWhenCancelled');

done();
