//@ requireOptions("--useTemporal=1")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrow(func, errorType, message) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
    if (message !== undefined)
        shouldBe(String(error), message);
}

let failures = [
    "",
    "+",
    "+2",
    "+2:00",
    "+24",
    "+23:",
    "20:00",
    "+20:0000",
    "+20:60",
    "+20:59:",
    "2059:00",
    "+2059:00",
    "+20:5900",
    "+20:59:00.0123456789",
    "local",
];

for (let text of failures) {
    shouldThrow(() => {
        new Temporal.TimeZone(text);
    }, RangeError);
}

shouldBe(new Temporal.TimeZone("+00:00").id, `+00:00`);
shouldBe(new Temporal.TimeZone("+01:00").id, `+01:00`);
shouldBe(new Temporal.TimeZone("+01:59").id, `+01:59`);
shouldBe(new Temporal.TimeZone("-01:59").id, `-01:59`);
shouldBe(new Temporal.TimeZone("-01:59:30").id, `-01:59:30`);
shouldBe(new Temporal.TimeZone("-01:59:30.12345").id, `-01:59:30.12345`);
shouldBe(new Temporal.TimeZone("-01:59:30.000001").id, `-01:59:30.000001`);
shouldBe(new Temporal.TimeZone("-01:59:30.123450000").id, `-01:59:30.12345`);
shouldBe(new Temporal.TimeZone("-01:59:30.000000001").id, `-01:59:30.000000001`);
shouldBe(new Temporal.TimeZone("-01:59:30.123456789").id, `-01:59:30.123456789`);

let tz = new Temporal.TimeZone("-01:59:30.123456789")
shouldBe(tz.id, String(tz));

shouldBe(new Temporal.TimeZone("Asia/Tokyo").id, `Asia/Tokyo`);
shouldBe(new Temporal.TimeZone("ASIA/TOKYO").id, `Asia/Tokyo`);
shouldBe(new Temporal.TimeZone("UTC").id, `UTC`);

shouldBe(new Temporal.TimeZone('UTC').id, `UTC`);
shouldBe(new Temporal.TimeZone('Africa/Cairo').id, `Africa/Cairo`);
shouldBe(new Temporal.TimeZone('america/VANCOUVER').id, `America/Vancouver`);
shouldBe(new Temporal.TimeZone('Asia/Katmandu').id, `Asia/Katmandu`);
shouldBe(new Temporal.TimeZone('-04:00').id, `-04:00`);
shouldBe(new Temporal.TimeZone('+0645').id, `+06:45`);

shouldBe(Temporal.TimeZone.from('UTC').id, `UTC`);
shouldBe(Temporal.TimeZone.from('Africa/Cairo').id, `Africa/Cairo`);
shouldBe(Temporal.TimeZone.from('Africa/Cairo').toString(), `Africa/Cairo`);
shouldBe(Temporal.TimeZone.from('Africa/Cairo').toJSON(), `Africa/Cairo`);
shouldBe(Temporal.TimeZone.from('america/VANCOUVER').id, `America/Vancouver`);
shouldBe(Temporal.TimeZone.from('Asia/Katmandu').id, `Asia/Katmandu`);
shouldBe(Temporal.TimeZone.from('-04:00').id, `-04:00`);
shouldBe(Temporal.TimeZone.from('+0645').id, `+06:45`);
shouldBe(Temporal.TimeZone.from('+0645').toString(), `+06:45`);
shouldBe(Temporal.TimeZone.from('+0645').toJSON(), `+06:45`);

// ISO 8601 string with time zone offset part
// FIXME: We will support Temporal Instant format in Temporal.TimeZone.from.
// shouldBe(Temporal.TimeZone.from('2020-01-14T00:31:00.065858086Z').id, ``);
// shouldBe(Temporal.TimeZone.from('2020-01-13T16:31:00.065858086-08:00').id, ``);
// shouldBe(Temporal.TimeZone.from('2020-01-13T16:31:00.065858086-08:00[America/Vancouver]').id, ``);

const user = {
  id: 775,
  username: 'robotcat',
  password: 'hunter2', // Note: Don't really store passwords like that
  userTimeZone: Temporal.TimeZone.from('Europe/Madrid')
};
const str = JSON.stringify(user);
shouldBe(str, `{"id":775,"username":"robotcat","password":"hunter2","userTimeZone":"Europe/Madrid"}`);
function reviver(key, value) {
  if (key.endsWith('TimeZone')) return Temporal.TimeZone.from(value);
  return value;
}
const parsed = JSON.parse(str, reviver);
shouldBe(parsed.userTimeZone instanceof Temporal.TimeZone, true);
shouldBe(parsed.userTimeZone.id, `Europe/Madrid`);
