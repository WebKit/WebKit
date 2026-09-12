promise_test(async t => {
  const res = new Response(new FormData());
  const fd = await res.formData();
  assert_true(fd instanceof FormData);
}, 'Consume empty response.formData() as FormData');

promise_test(async t => {
  const req = new Request('about:blank', {
    method: 'POST',
    body: new FormData()
  });
  const fd = await req.formData();
  assert_true(fd instanceof FormData);
}, 'Consume empty request.formData() as FormData');

promise_test(async t => {
  let formdata = new FormData();
  formdata.append('foo', new Blob([JSON.stringify({ bar: "baz", })], { type: "application/json" }));
  let blob = await new Response(formdata).blob();
  let body = await blob.text();
  blob = new Blob([body.toLowerCase()], { type: blob.type.toLowerCase() });
  let formdataWithLowercaseBody = await new Response(blob).formData();
  assert_true(formdataWithLowercaseBody.has("foo"));
  assert_equals(formdataWithLowercaseBody.get("foo").type, "application/json");
}, 'Consume multipart/form-data headers case-insensitively');

// The tests below exercise the multipart/form-data chunk serializer, which is
// what turns a FormData object into a request or response body.

const kBoundaryPrefix = "multipart/form-data; boundary=";

// Returns the boundary from a Response created from a FormData object.
function boundaryOf(response) {
  const contentType = response.headers.get("Content-Type");
  assert_true(contentType.startsWith(kBoundaryPrefix),
              `unexpected Content-Type: ${contentType}`);
  return contentType.substring(kBoundaryPrefix.length);
}

// Reads a body as bytes and isomorphic decodes it, so that expectations can be
// written with escapes such as "\xC3\xA1" for individual bytes.
async function isomorphicText(response) {
  const bytes = new Uint8Array(await response.arrayBuffer());
  return Array.from(bytes, byte => String.fromCharCode(byte)).join("");
}

// `lines` is called with the generated boundary and returns the lines the
// payload is expected to consist of, joined by CRLF.
function serializationTest(description, populate, lines) {
  promise_test(async () => {
    const formData = new FormData();
    populate(formData);
    const response = new Response(formData);
    const boundary = boundaryOf(response);
    assert_equals(await isomorphicText(response), lines(boundary).join("\r\n"));
  }, description);
}

// Builds the expected payload for a FormData object with a single entry.
// `filename` and `type` are only included when given, matching the serializer,
// which only emits them for File values.
function onePart(boundary, { name, filename, type, value }) {
  const disposition = filename === undefined
      ? `Content-Disposition: form-data; name="${name}"`
      : `Content-Disposition: form-data; name="${name}"; filename="${filename}"`;
  return [
    `--${boundary}`,
    disposition,
    ...(type === undefined ? [] : [`Content-Type: ${type}`]),
    "",
    value,
    `--${boundary}--`,
    "",
  ];
}

test(() => {
  const boundary = boundaryOf(new Response(new FormData()));
  assert_greater_than(boundary.length, 26, "boundary is longer than 26 bytes");
  assert_less_than(boundary.length, 71, "boundary is shorter than 71 bytes");
  assert_regexp_match(boundary, /^[0-9A-Za-z'\-_]+$/, "boundary bytes");
}, "multipart/form-data boundary is well-formed");

test(() => {
  const formData = new FormData();
  assert_not_equals(boundaryOf(new Response(formData)),
                    boundaryOf(new Response(formData)),
                    "two payloads should not share a boundary");
}, "multipart/form-data boundary is randomly generated");

serializationTest("Serialize an empty FormData object",
  formData => {},
  boundary => [`--${boundary}--`, ""]);

serializationTest("Serialize string entries in order, keeping duplicates",
  formData => {
    formData.append("a", "1");
    formData.append("b", "2");
    formData.append("a", "3");
  },
  boundary => [
    `--${boundary}`,
    `Content-Disposition: form-data; name="a"`,
    "",
    "1",
    `--${boundary}`,
    `Content-Disposition: form-data; name="b"`,
    "",
    "2",
    `--${boundary}`,
    `Content-Disposition: form-data; name="a"`,
    "",
    "3",
    `--${boundary}--`,
    "",
  ]);

serializationTest("Serialize a File entry",
  formData => formData.append("a", new File(["contents"], "b.txt", { type: "text/plain" })),
  boundary => onePart(boundary, { name: "a", filename: "b.txt", type: "text/plain", value: "contents" }));

serializationTest("Serialize a File entry without a type",
  formData => formData.append("a", new File(["contents"], "b.txt")),
  boundary => onePart(boundary, { name: "a", filename: "b.txt", type: "application/octet-stream", value: "contents" }));

serializationTest("Serialize an empty File entry",
  formData => formData.append("a", new File([], "b.txt", { type: "text/plain" })),
  boundary => onePart(boundary, { name: "a", filename: "b.txt", type: "text/plain", value: "" }));

serializationTest("Serialize a Blob entry",
  formData => formData.append("a", new Blob(["contents"], { type: "text/plain" })),
  boundary => onePart(boundary, { name: "a", filename: "blob", type: "text/plain", value: "contents" }));

serializationTest("Serialize a Blob entry without a type",
  formData => formData.append("a", new Blob(["contents"])),
  boundary => onePart(boundary, { name: "a", filename: "blob", type: "application/octet-stream", value: "contents" }));

serializationTest("Serialize a Blob entry with a filename argument",
  formData => formData.append("a", new Blob(["contents"], { type: "text/plain" }), "b.txt"),
  boundary => onePart(boundary, { name: "a", filename: "b.txt", type: "text/plain", value: "contents" }));

serializationTest("Serialize a File entry with a filename argument",
  formData => formData.append("a", new File(["contents"], "b.txt", { type: "text/plain" }), "c.txt"),
  boundary => onePart(boundary, { name: "a", filename: "c.txt", type: "text/plain", value: "contents" }));

// Entry names have lone CR and LF normalized to CRLF, and then 0x0A (LF),
// 0x0D (CR) and 0x22 (") escaped. Nothing else is escaped.
const nameCases = [
  ["a\nb", "a%0D%0Ab"],
  ["a\rb", "a%0D%0Ab"],
  ["a\r\nb", "a%0D%0Ab"],
  ["a\n\rb", "a%0D%0A%0D%0Ab"],
  ['a"b', "a%22b"],
  ["a'b", "a'b"],
  ["a\\b", "a\\b"],
  ["a%0Ab", "a%0Ab"],
  ["áb", "\xC3\xA1b"],
];
for (const [name, serialized] of nameCases) {
  serializationTest(`Serialize name ${format_value(name)}`,
    formData => formData.append(name, "x"),
    boundary => onePart(boundary, { name: serialized, value: "x" }));
}

// Filenames are escaped the same way, but are not newline normalized first.
const filenameCases = [
  ["b\nc", "b%0Ac"],
  ["b\rc", "b%0Dc"],
  ["b\r\nc", "b%0D%0Ac"],
  ["b\n\rc", "b%0A%0Dc"],
  ['b"c', "b%22c"],
  ["b'c", "b'c"],
  ["b\\c", "b\\c"],
  ["ə.txt", "\xC9\x99.txt"],
];
for (const [filename, serialized] of filenameCases) {
  serializationTest(`Serialize filename ${format_value(filename)}`,
    formData => formData.append("a", new File([], filename, { type: "text/plain" })),
    boundary => onePart(boundary, { name: "a", filename: serialized, type: "text/plain", value: "" }));
}

// String values have lone CR and LF normalized to CRLF, but are not escaped.
const valueCases = [
  ["b\nc", "b\r\nc"],
  ["b\rc", "b\r\nc"],
  ["b\r\nc", "b\r\nc"],
  ["b\n\rc", "b\r\n\r\nc"],
  ['b"c', 'b"c'],
  ["ç", "\xC3\xA7"],
];
for (const [value, serialized] of valueCases) {
  serializationTest(`Serialize value ${format_value(value)}`,
    formData => formData.append("a", value),
    boundary => onePart(boundary, { name: "a", value: serialized }));
}

promise_test(async () => {
  const formData = new FormData();
  formData.append("a", "1");
  const response = new Response(formData);
  formData.append("b", "2");
  const boundary = boundaryOf(response);
  assert_equals(await isomorphicText(response),
                onePart(boundary, { name: "a", value: "1" }).join("\r\n"));
}, "Serializing a FormData object takes a snapshot of its entry list");

promise_test(async () => {
  const formData = new FormData();
  formData.append("a", "1");
  formData.append("a", "2");
  formData.append("b", "3");
  const parsed = await new Response(formData).formData();
  assert_array_equals(parsed.getAll("a"), ["1", "2"]);
  assert_array_equals(parsed.getAll("b"), ["3"]);
  assert_array_equals([...parsed.keys()], ["a", "a", "b"]);
}, "Round trip string entries");

promise_test(async () => {
  const formData = new FormData();
  formData.append("a", new File(["contents"], "b.txt", { type: "text/plain" }));
  formData.append("c", new Blob(["more"], { type: "text/html" }));
  const parsed = await new Response(formData).formData();

  const file = parsed.get("a");
  assert_true(file instanceof File, "a should be a File");
  assert_equals(file.name, "b.txt", "a's filename");
  assert_equals(file.type, "text/plain", "a's type");
  assert_equals(await file.text(), "contents", "a's contents");

  const blob = parsed.get("c");
  assert_true(blob instanceof File, "c should be a File");
  assert_equals(blob.name, "blob", "c's filename");
  assert_equals(blob.type, "text/html", "c's type");
  assert_equals(await blob.text(), "more", "c's contents");
}, "Round trip File and Blob entries");

// Escaping a name or a filename is one-way: parsing does not turn %0A, %0D and
// %22 back into LF, CR and ". It cannot, because 0x25 (%) is not escaped, so a
// name that already contains one of those sequences serializes to the same
// bytes as the name containing the character it stands for.
promise_test(async () => {
  const formData = new FormData();
  formData.append('a"b', "c");
  formData.append("d", new File([], 'e"f.txt', { type: "text/plain" }));
  const parsed = await new Response(formData).formData();
  assert_array_equals([...parsed.keys()], ["a%22b", "d"], "entry names");
  assert_equals(parsed.get("a%22b"), "c", "value of the escaped name");
  assert_equals(parsed.get("d").name, "e%22f.txt", "escaped filename");
}, "A double quote in a name and a filename stays escaped when parsed back");

promise_test(async () => {
  const formData = new FormData();
  formData.append("a\nb", "c\nd");
  const parsed = await new Response(formData).formData();
  // The name is escaped, but the value is only newline normalized.
  assert_array_equals([...parsed.keys()], ["a%0D%0Ab"], "entry names");
  assert_equals(parsed.get("a%0D%0Ab"), "c\r\nd", "value");
}, "A newline in a name stays escaped when parsed back, a newline in a value is normalized");
