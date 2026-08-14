// META: global=window,worker
// META: script=resources/webtransport-test-helpers.sub.js
// Tests for the WebTransportOptions headers option and the responseHeaders
// attribute.
// https://w3c.github.io/webtransport/#dom-webtransportoptions-headers
//
// The constructor tests validate constructor behavior only and do not require
// a WebTransport server connection. They use https://localhost:0/ as
// the URL to avoid triggering real connection attempts. The remaining tests
// connect to the webtransport-h3 test server.

const TEST_URL = 'https://localhost:0/';

function createAndClose(url, options) {
  const wt = new WebTransport(url, options);
  wt.ready.catch(() => {});
  wt.closed.catch(() => {});
  wt.close();
}

test(() => {
  assert_throws_js(TypeError, () => new WebTransport(
    TEST_URL, { headers: { 'wt-available-protocols': 'test' } }));
}, 'Setting wt-available-protocols header should throw TypeError');

test(() => {
  assert_throws_js(TypeError, () => new WebTransport(
    TEST_URL, { headers: { 'WT-Available-Protocols': 'test' } }));
}, 'Setting wt-available-protocols header should throw TypeError (case-insensitive)');

test(() => {
  assert_throws_js(TypeError, () => new WebTransport(
    TEST_URL, { headers: { 'Wt-AVAILABLE-protocols': 'test' } }));
}, 'Setting wt-available-protocols header should throw TypeError (mixed case)');

test(() => {
  createAndClose(TEST_URL,
    { headers: { 'Host': 'evil.example.com' } });
}, 'Forbidden request headers should be silently dropped');

test(() => {
  createAndClose(TEST_URL, { headers: {} });
}, 'Empty headers object should be accepted');

test(() => {
  createAndClose(TEST_URL,
    { headers: { 'x-custom-header': 'custom-value' } });
}, 'Custom headers should be accepted');

test(() => {
  createAndClose(TEST_URL,
    { headers: {
        'x-grpc-method': '/service/Method',
        'content-type': 'application/grpc+proto',
        'x-custom': 'value'
    } });
}, 'Multiple custom headers should be accepted');

test(() => {
  createAndClose(TEST_URL,
    { headers: [['x-foo', 'bar'], ['x-baz', 'qux']] });
}, 'Headers as sequence of sequences should be accepted');

test(() => {
  createAndClose(TEST_URL,
    { headers: [['x-dup', 'one'], ['x-dup', 'two']] });
}, 'Duplicate header names in sequence form should be accepted');

test(() => {
  createAndClose(TEST_URL,
    { headers: { 'X-Custom': 'value' } });
}, 'Header names should be lowercased');

test(() => {
  createAndClose(TEST_URL,
    { headers: [['foo', 'bar'], ['Foo', 'baz']] });
}, 'Mixed-case duplicate header names should be lowercased and preserved');

test(() => {
  assert_throws_js(TypeError, () => new WebTransport(
    TEST_URL, { headers: [['only-one-element']] }));
}, 'Sequence with wrong inner length should throw TypeError');

test(() => {
  assert_throws_js(TypeError, () => new WebTransport(
    TEST_URL, { headers: { 'bad name': 'value' } }));
}, 'Invalid header name should throw TypeError');

test(() => {
  assert_throws_js(TypeError, () => new WebTransport(
    TEST_URL, { headers: { 'x-custom': 'bad\r\nvalue' } }));
}, 'Header value with CR/LF should throw TypeError');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo-request-headers.py'));
  t.add_cleanup(() => wt.close());
  assert_equals(wt.responseHeaders, null,
    'responseHeaders is null synchronously after construction');
  await wt.ready;
}, 'responseHeaders is null before the connection is established');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo-request-headers.py'), {
    headers: {
      'X-Custom-Header': '  custom-value  ',
      'x-other-header': 'other-value',
      'Host': 'evil.example.com',
    },
  });
  await wt.ready;

  // Read incoming unidirectional stream for echoed request headers.
  const streams = await wt.incomingUnidirectionalStreams;
  const stream_reader = streams.getReader();
  const { value: recv_stream } = await stream_reader.read();
  stream_reader.releaseLock();
  const request_headers = await read_stream_as_json(recv_stream);

  assert_equals(request_headers['x-custom-header'], 'custom-value',
    'header name should be lowercased and value trimmed');
  assert_equals(request_headers['x-other-header'], 'other-value');
  assert_equals(request_headers['host'], undefined,
    'forbidden request headers should not be sent');
}, 'Custom request headers are received by the server');

promise_test(async t => {
  const wt = new WebTransport(
    webtransport_url('custom-response.py?x-custom-response=custom-response-value'));
  await wt.ready;

  assert_not_equals(wt.responseHeaders, null,
    'responseHeaders should not be null after ready');
  assert_true(wt.responseHeaders instanceof Headers);
  assert_equals(wt.responseHeaders.get('x-custom-response'),
    'custom-response-value');
}, 'responseHeaders exposes headers from the server CONNECT response');

promise_test(async t => {
  const wt = new WebTransport(
    webtransport_url('custom-response.py?set-cookie=a%3Db&x-safe-header=yes'));
  await wt.ready;

  assert_equals(wt.responseHeaders.get('x-safe-header'), 'yes',
    'non-forbidden response headers should be exposed');
  assert_equals(wt.responseHeaders.get('set-cookie'), null,
    'forbidden response header names should not be exposed via get()');
  const names = [...wt.responseHeaders.keys()];
  assert_false(names.includes('set-cookie'),
    'forbidden response header names should not be exposed via iteration');
}, 'Forbidden response header names are not exposed in responseHeaders');

promise_test(async t => {
  const wt = new WebTransport(
    webtransport_url('custom-response.py?wt-protocol=foo'));
  t.add_cleanup(() => wt.close());
  await wt.ready;

  assert_false(wt.responseHeaders.has('wt-protocol'),
    'wt-protocol must not be exposed via has()');
  const names = [...wt.responseHeaders.keys()];
  assert_false(names.includes('wt-protocol'),
    'wt-protocol must not be exposed via iteration');
}, 'wt-protocol is stripped from responseHeaders');
