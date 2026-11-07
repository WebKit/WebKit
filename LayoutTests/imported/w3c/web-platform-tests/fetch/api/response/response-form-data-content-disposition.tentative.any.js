// Fetch does not define how the Content-Disposition value of a
// multipart/form-data part is parsed; that step is marked XXX in the standard,
// pending a Content-Disposition parser, ideally the same one downloads need.
// These tests are therefore tentative: they record the behaviour Chromium,
// Gecko and WebKit already agree on, which is what any such parser will have to
// keep working. All three treat the value as a disposition type followed by a
// list of parameters, so optional whitespace, unquoted values and unknown
// parameters are all accepted.
//
// Only the entry name is asserted, since that is the part the three engines
// demonstrably agree on. Cases they disagree about are deliberately left out:
// an uppercase FORM-DATA disposition type, an unterminated quoted name, and
// which of two Content-Disposition headers wins. Those need resolving before
// the parser can be specified, and how a quoted string escapes 0x22 (") is the
// central question.

const boundary = "boundary";

// Builds a payload with a single part; multipart/form-data uses CRLF as its
// line terminator throughout.
function onePart(header) {
  return ["--boundary", header, "", "value", "--boundary--", ""].join("\r\n");
}

function parse(body) {
  const contentType = `multipart/form-data; boundary=${boundary}`;
  return new Response(body, { headers: [["Content-Type", contentType]] }).formData();
}

const cases = [
  ["no space after form-data;", 'Content-Disposition: form-data;name="a"'],
  ["unquoted name", "Content-Disposition: form-data; name=a"],
  ["unknown parameter after name", 'Content-Disposition: form-data; name="a"; charset=utf-8'],
  ["unknown parameter after filename",
   'Content-Disposition: form-data; name="a"; filename="f"; size=1'],
];

for (const [description, header] of cases) {
  promise_test(async t => {
    const formData = await parse(onePart(header));
    assert_array_equals([...formData.keys()], ["a"], "entry names");
  }, `Parse ${description}`);
}
