if (self.importScripts) {
  importScripts('/resources/testharness.js');
}

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST'});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, '');
        });
  }, 'MIME type for Blob from empty body');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST', headers: [['Content-Type', 'Mytext/Plain']]});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'mytext/plain');
        });
  }, 'MIME type for Blob from empty body with Content-Type');

// The 5 following tests are coming from Chromium see fetch/script-tests/request.js

// Tests for MIME types.
promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST', body: new Blob([''])});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, '');
          assert_equals(req.headers.get('Content-Type'), '');
        });
  }, 'MIME type for Blob');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'})});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'text/plain');
          assert_equals(req.headers.get('Content-Type'), 'text/plain');
        });
  }, 'MIME type for Blob with non-empty type');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'}),
                           headers: [['Content-Type', 'Text/Html']]});
    var clone = req.clone();
    return Promise.all([req.blob(), clone.blob()])
      .then(function(blobs) {
          assert_equals(blobs[0].type, 'text/html');
          assert_equals(blobs[1].type, 'text/html');
          assert_equals(req.headers.get('Content-Type'), 'Text/Html');
          assert_equals(clone.headers.get('Content-Type'), 'Text/Html');
        });
  }, 'Extract a MIME type with clone');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'})});
    req.headers.set('Content-Type', 'Text/Html');
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'text/plain');
          assert_equals(req.headers.get('Content-Type'), 'Text/Html');
        });
  },
  'MIME type unchanged if headers are modified after Request() constructor');

promise_test(function(t) {
    var req = new Request('http://localhost/',
                          {method: 'POST',
                           body: new Blob([''], {type: 'Text/Plain'}),
                           headers: [['Content-Type', 'Text/Html']]});
    return req.blob()
      .then(function(blob) {
          assert_equals(blob.type, 'text/html');
          assert_equals(req.headers.get('Content-Type'), 'Text/Html');
        });
  }, 'Extract a MIME type (1)');

done();
