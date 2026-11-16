// META: global=window,worker
// META: script=resources/webtransport-test-helpers.sub.js

// Tests for WebTransport.exportKeyingMaterial(), which exports keying material
// derived from the underlying TLS 1.3 connection (RFC 5705 style exporters).
// https://www.w3.org/TR/webtransport/#dom-webtransport-exportkeyingmaterial

const encoder = new TextEncoder();

// exportKeyingMaterial() resolves with the derived bytes. Normalize the result
// to a Uint8Array so the tests work whether an ArrayBuffer or a typed array
// view is returned.
function toBytes(result) {
  return new Uint8Array(result);
}

function arraysEqual(a, b) {
  if (a.length !== b.length)
    return false;
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i])
      return false;
  }
  return true;
}

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo.py'));
  await wt.ready;
  t.add_cleanup(() => wt.close());

  const label = encoder.encode('EXPORTER-webtransport');
  const context = encoder.encode('context');
  const result = await wt.exportKeyingMaterial(label, context, 32);
  assert_equals(result.byteLength, 32,
      'result length should match the requested outputLength');
}, 'WebTransport exportKeyingMaterial returns keying material of the requested length');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo.py'));
  await wt.ready;
  t.add_cleanup(() => wt.close());

  const label = encoder.encode('EXPORTER-lengths');
  const context = encoder.encode('context');
  for (const length of [1, 16, 64, 4096]) {
    const result = await wt.exportKeyingMaterial(label, context, length);
    assert_equals(result.byteLength, length,
        `result length should be ${length}`);
  }
}, 'WebTransport exportKeyingMaterial honors the requested output length');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo.py'));
  await wt.ready;
  t.add_cleanup(() => wt.close());

  const label = encoder.encode('EXPORTER-determinism');
  const context = encoder.encode('context');
  const first = toBytes(await wt.exportKeyingMaterial(label, context, 32));
  const second = toBytes(await wt.exportKeyingMaterial(label, context, 32));
  assert_array_equals(second, first,
      'identical label, context, and length should produce identical output');
}, 'WebTransport exportKeyingMaterial is deterministic for identical inputs');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo.py'));
  await wt.ready;
  t.add_cleanup(() => wt.close());

  const context = encoder.encode('context');
  const a = toBytes(await wt.exportKeyingMaterial(
      encoder.encode('EXPORTER-label-a'), context, 32));
  const b = toBytes(await wt.exportKeyingMaterial(
      encoder.encode('EXPORTER-label-b'), context, 32));
  assert_false(arraysEqual(a, b),
      'different labels should produce different keying material');
}, 'WebTransport exportKeyingMaterial returns different material for different labels');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo.py'));
  await wt.ready;
  t.add_cleanup(() => wt.close());

  const label = encoder.encode('EXPORTER-context');
  const a = toBytes(await wt.exportKeyingMaterial(
      label, encoder.encode('context-a'), 32));
  const b = toBytes(await wt.exportKeyingMaterial(
      label, encoder.encode('context-b'), 32));
  assert_false(arraysEqual(a, b),
      'different contexts should produce different keying material');
}, 'WebTransport exportKeyingMaterial returns different material for different contexts');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo.py'));
  await wt.ready;
  t.add_cleanup(() => wt.close());

  const label = encoder.encode('EXPORTER-range');
  const context = encoder.encode('context');

  // A label longer than 255 bytes is out of range.
  await promise_rejects_js(t, RangeError,
      wt.exportKeyingMaterial(new Uint8Array(256), context, 32),
      'label longer than 255 bytes');
  // A context longer than 255 bytes is out of range.
  await promise_rejects_js(t, RangeError,
      wt.exportKeyingMaterial(label, new Uint8Array(256), 32),
      'context longer than 255 bytes');
  // An outputLength of 0 is out of range.
  await promise_rejects_js(t, RangeError,
      wt.exportKeyingMaterial(label, context, 0),
      'outputLength of 0');
  // An outputLength greater than 4096 is out of range.
  await promise_rejects_js(t, RangeError,
      wt.exportKeyingMaterial(label, context, 4097),
      'outputLength greater than 4096');
}, 'WebTransport exportKeyingMaterial rejects out-of-range arguments with RangeError');

promise_test(async t => {
  const wt = new WebTransport(webtransport_url('echo.py'));
  await wt.ready;
  wt.close();
  await wt.closed;

  await promise_rejects_dom(t, 'InvalidStateError',
      wt.exportKeyingMaterial(
          encoder.encode('EXPORTER-closed'), encoder.encode('context'), 32));
}, 'WebTransport exportKeyingMaterial rejects with InvalidStateError after the session is closed');

promise_test(async t => {
  const wt = new WebTransport('https://webtransport.invalid/');
  wt.closed.catch(() => {});
  // Wait for the connection to fail before exporting keying material.
  await wt.ready.catch(() => {});

  await promise_rejects_dom(t, 'InvalidStateError',
      wt.exportKeyingMaterial(
          encoder.encode('EXPORTER-failed'), encoder.encode('context'), 32));
}, 'WebTransport exportKeyingMaterial rejects with InvalidStateError for a failed session');
