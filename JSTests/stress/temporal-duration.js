//@ requireOptions("--useTemporal=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotThrow(func) {
    func();
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }

    if (!(error instanceof errorType))
        throw new Error(`Expected ${errorType.name}!`);
}

shouldBe(Temporal.Duration instanceof Function, true);
shouldBe(Temporal.Duration.length, 0);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.Duration, 'prototype').writable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.Duration, 'prototype').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Temporal.Duration, 'prototype').configurable, false);
shouldThrow(() => Temporal.Duration(), TypeError);
shouldBe(Temporal.Duration.prototype.constructor, Temporal.Duration);

shouldBe(new Temporal.Duration() instanceof Temporal.Duration, true);
{
    class DerivedDuration extends Temporal.Duration {}

    const dd = new DerivedDuration();
    shouldBe(dd instanceof DerivedDuration, true);
    shouldBe(dd instanceof Temporal.Duration, true);
    shouldBe(dd.negated, Temporal.Duration.prototype.negated);
    shouldBe(Object.getPrototypeOf(dd), DerivedDuration.prototype);
    shouldBe(Object.getPrototypeOf(DerivedDuration.prototype), Temporal.Duration.prototype);
}

shouldThrow(() => new Temporal.Duration(1, -1), RangeError);
shouldThrow(() => new Temporal.Duration(Infinity), RangeError);

const fields = ['years', 'months', 'weeks', 'days', 'hours', 'minutes', 'seconds', 'milliseconds', 'microseconds', 'nanoseconds'];
const zero = new Temporal.Duration();
const pos = new Temporal.Duration(1,2,3,4,5,6,7,8,9,10);
const neg = new Temporal.Duration(-1,-2,-3,-4,-5,-6,-7,-8,-9,-10);

fields.forEach((field, i) => {
    shouldThrow(() => Temporal.Duration.prototype[field], TypeError);
    shouldBe(zero[field], 0);
    shouldBe(pos[field], i + 1);
    shouldBe(neg[field], -i - 1);
});

shouldThrow(() => Temporal.Duration.prototype.sign, TypeError);
shouldBe(zero.sign, 0);
shouldBe(pos.sign, 1);
shouldBe(neg.sign, -1);

shouldThrow(() => Temporal.Duration.prototype.blank, TypeError);
shouldBe(zero.blank, true);
shouldBe(pos.blank, false);
shouldBe(neg.blank, false);

shouldBe(Temporal.Duration.from.length, 1);
shouldThrow(() => Temporal.Duration.from(), TypeError);
shouldThrow(() => Temporal.Duration.from({}), TypeError);
{
    const badStrings = [
        '', '+', '+P', '-P', '+-P',
        'P', 'P1', 'PT', 'P1YT', 'PT1',
        'PT20.S', 'PT.20S',
        'P1W1Y',  'PT1S1M',
        'P1.1Y', 'PT1.1H1M', 'PT1.1M1S',
        'PT1.1111111111H', 'PT1.1111111111M', 'PT1.1111111111S',
        `P${Array(309).fill(9).join('')}Y`,
        '\u2212p1y2m3w4dt5h6m7,008009010s'
    ];
    for (const badString of badStrings)
        shouldThrow(() => Temporal.Duration.from(badString), RangeError);
    
    const fromString = Temporal.Duration.from('P1Y2M3W4DT5H6M7.008009010S');

    const durationLike = {};
    fields.forEach((field, i) => { durationLike[field] = i + 1; });
    const fromDurationLike = Temporal.Duration.from(durationLike);

    for (const field of fields) {
        shouldBe(fromString[field], pos[field]);
        shouldBe(fromDurationLike[field], pos[field]);
    }
}
shouldThrow(() => Temporal.Duration.from(`P${Array(308).fill(9).join('')}Y`), RangeError);
shouldBe(Temporal.Duration.from('+P1Y2DT3H4.0S').toString(), 'P1Y2DT3H4S');
shouldBe(Temporal.Duration.from('PT1.03125H').toString(), 'PT1H1M52.5S');
shouldBe(Temporal.Duration.from('PT1.03125M').toString(), 'PT1M1.875S');

const posAbsolute = new Temporal.Duration(0,0,0,1,2,3,4,5,6,7);

shouldBe(Temporal.Duration.compare.length, 2);
shouldThrow(() => Temporal.Duration.compare(), TypeError);
shouldThrow(() => Temporal.Duration.compare({}), TypeError);
shouldThrow(() => Temporal.Duration.compare(zero), TypeError);
shouldThrow(() => Temporal.Duration.compare(zero, {}), TypeError);
shouldThrow(() => Temporal.Duration.compare({ years: 1 }, zero), RangeError);
shouldThrow(() => Temporal.Duration.compare({ months: 1 }, zero), RangeError);
shouldThrow(() => Temporal.Duration.compare({ weeks: 1 }, zero), RangeError);
shouldThrow(() => Temporal.Duration.compare(zero, { years: 1 }), RangeError);
shouldThrow(() => Temporal.Duration.compare(zero, { months: 1 }), RangeError);
shouldThrow(() => Temporal.Duration.compare(zero, { weeks: 1 }), RangeError);
shouldBe(Temporal.Duration.compare(posAbsolute, posAbsolute), 0);
shouldBe(Temporal.Duration.compare(posAbsolute, zero), 1);
shouldBe(Temporal.Duration.compare(zero, posAbsolute), -1);
shouldBe(Temporal.Duration.compare('PT86400S', 'P1D'), 0);

shouldBe(Temporal.Duration.prototype.with.length, 1);
shouldThrow(() => Temporal.Duration.prototype.with.call({}, { years: 1 }), TypeError);
shouldThrow(() => zero.with(), TypeError);
shouldThrow(() => zero.with({}), TypeError);
shouldThrow(() => zero.with({ years: 1.1 }), RangeError);
shouldThrow(() => pos.with({ years: -1 }), RangeError);
{
    const fullyUpdated = pos.with(neg);
    for (const field of fields) {
        shouldBe(zero.with({ [field]: 1 })[field], 1);
        shouldBe(fullyUpdated[field], neg[field]);
    }
}

shouldBe(Temporal.Duration.prototype.negated.length, 0);
shouldThrow(() => Temporal.Duration.prototype.negated.call({}), TypeError);
{
    const negatedZero = zero.negated();
    const negatedPos = pos.negated();
    const negatedNeg = neg.negated();
    for (const field of fields) {
        shouldBe(negatedZero[field], zero[field]);
        shouldBe(negatedNeg[field], pos[field]);
        shouldBe(negatedPos[field], neg[field]);
    }
}

shouldBe(Temporal.Duration.prototype.abs.length, 0);
shouldThrow(() => Temporal.Duration.prototype.abs.call({}), TypeError);
{
    const absZero = zero.abs();
    const absPos = pos.abs();
    const absNeg = neg.abs();
    for (const field of fields) {
        shouldBe(absZero[field], zero[field]);
        shouldBe(absPos[field], pos[field]);
        shouldBe(absNeg[field], pos[field]);
    }
}

for (const method of ['add', 'subtract']) {
    shouldBe(Temporal.Duration.prototype[method].length, 1);
    shouldThrow(() => Temporal.Duration.prototype[method].call({ hours: 1 }), TypeError);
    shouldThrow(() => zero[method](), TypeError);
    shouldThrow(() => zero[method]({}), TypeError);
    shouldThrow(() => zero[method]({ years: 1 }), RangeError);
    shouldThrow(() => zero[method]({ months: 1 }), RangeError);
    shouldThrow(() => zero[method]({ weeks: 1 }), RangeError);
    shouldThrow(() => pos[method]({ seconds: 1 }), RangeError);
}
{
    const posDurationLike = { days: 1, hours: 2, minutes: 3, seconds: 4, milliseconds: 5, microseconds: 6, nanoseconds: 7 };
    const negString = '-P1DT2H3M4.005006007S';

    const addZero = posAbsolute.add(zero);
    const addPos = posAbsolute.add(posDurationLike);
    const addNeg = posAbsolute.add(negString);
    const subZero = posAbsolute.subtract(zero);
    const subPos = posAbsolute.subtract(posDurationLike);
    const subNeg = posAbsolute.subtract(negString);
    for (const field of fields) {
        shouldBe(addZero[field], posAbsolute[field]);
        shouldBe(addPos[field], 2 * posAbsolute[field]);
        shouldBe(addNeg[field], 0);
        shouldBe(subZero[field], posAbsolute[field]);
        shouldBe(subPos[field], 0);
        shouldBe(subNeg[field], 2 * posAbsolute[field]);
    }
}
shouldBe(Temporal.Duration.from('P1DT13H31M31S').add('P1DT13H31M31S').toString(), 'P3DT3H3M2S');
shouldBe(Temporal.Duration.from('-PT1M59S').subtract('PT1M59S').toString(), '-PT3M58S');

shouldBe(Temporal.Duration.prototype.round.length, 1);
shouldThrow(() => Temporal.Duration.prototype.round.call({}), TypeError);
shouldThrow(() => zero.round(), TypeError);
shouldThrow(() => zero.round({}), RangeError);
shouldThrow(() => zero.round({ smallestUnit: 'bogus' }), RangeError);
shouldThrow(() => zero.round({ largestUnit: 'bogus' }), RangeError);
shouldThrow(() => zero.round({ smallestUnit: 'hour', largestUnit: 'nanosecond' }), RangeError);
shouldThrow(() => zero.round({ smallestUnit: 'nanosecond', roundingMode: 'bogus' }), RangeError);
shouldThrow(() => zero.round({ smallestUnit: 'nanosecond', roundingIncrement: 3 }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1Y').round({ largestUnit: 'seconds' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1M').round({ largestUnit: 'seconds' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1W').round({ largestUnit: 'seconds' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1D').round({ largestUnit: 'months' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1D').round({ largestUnit: 'weeks' }), RangeError);
shouldBe(posAbsolute.round('day').toString(), 'P1D');
shouldBe(posAbsolute.round({ largestUnit: 'day' }).toString(), 'P1DT2H3M4.005006007S');
shouldBe(posAbsolute.round({ largestUnit: 'auto' }).toString(), 'P1DT2H3M4.005006007S');
shouldBe(posAbsolute.round({ smallestUnit: 'day' }).toString(), 'P1D');
shouldBe(posAbsolute.round({ smallestUnit: 'second' }).toString(), 'P1DT2H3M4S');
shouldBe(posAbsolute.round({ largestUnit: 'hour', smallestUnit: 'second' }).toString(), 'PT26H3M4S');
shouldBe(posAbsolute.round({ largestUnit: 'minute', smallestUnit: 'second' }).toString(), 'PT1563M4S');
shouldBe(posAbsolute.round({ largestUnit: 'second', smallestUnit: 'millisecond' }).toString(), 'PT93784.005S');
shouldBe(posAbsolute.round({ largestUnit: 'millisecond', smallestUnit: 'microsecond' }).toString(), 'PT93784.005006S');
shouldBe(posAbsolute.round({ largestUnit: 'microsecond', smallestUnit: 'microsecond' }).toString(), 'PT93784.005006S');
shouldBe(posAbsolute.round({ largestUnit: 'nanosecond' }).toString(), 'PT93784.005006007S');
shouldBe(posAbsolute.round({ smallestUnit: 'second', roundingIncrement: 30 }).toString(), 'P1DT2H3M');
shouldBe(posAbsolute.round({ smallestUnit: 'second', roundingIncrement: 30, roundingMode: 'ceil' }).toString(), 'P1DT2H3M30S');
shouldBe(Temporal.Duration.from('-PT31S').round({ smallestUnit: 'second', roundingIncrement: 30 }).toString(), '-PT30S');
shouldBe(Temporal.Duration.from('-PT31S').round({ smallestUnit: 'second', roundingIncrement: 30, roundingMode: 'floor' }).toString(), '-PT60S');
shouldBe(Temporal.Duration.from('-PT45S').round({ smallestUnit: 'second', roundingIncrement: 30 }).toString(), '-PT60S');
shouldBe(Temporal.Duration.from('-PT45S').round({ smallestUnit: 'second', roundingIncrement: 30, roundingMode: 'trunc' }).toString(), '-PT30S');

shouldBe(Temporal.Duration.prototype.total.length, 1);
shouldThrow(() => Temporal.Duration.prototype.total.call({}), TypeError);
shouldThrow(() => zero.total(), TypeError);
shouldThrow(() => zero.total({}), RangeError);
shouldThrow(() => Temporal.Duration.from('P1Y').total({ unit: 'seconds' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1M').total({ unit: 'seconds' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1W').total({ unit: 'seconds' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1D').total({ unit: 'months' }), RangeError);
shouldThrow(() => Temporal.Duration.from('P1D').total({ unit: 'weeks' }), RangeError);
shouldBe(posAbsolute.total('days'), 1.0854630209028588);
shouldBe(posAbsolute.total({ unit: 'days' }), 1.0854630209028588);
shouldBe(posAbsolute.total({ unit: 'hours' }), 26.051112501668612);
shouldBe(posAbsolute.total({ unit: 'minutes' }), 1563.0667501001167);
shouldBe(posAbsolute.total({ unit: 'seconds' }), 93784.005006007);
shouldBe(posAbsolute.total({ unit: 'milliseconds' }), 93784005.006007);
shouldBe(posAbsolute.total({ unit: 'microseconds' }), 93784005006.007);
shouldBe(posAbsolute.total({ unit: 'nanoseconds' }), 93784005006007);
shouldBe(Temporal.Duration.from('-PT123456789S').total({ unit: 'day' }), -1428.8980208333332);

// At present, toLocaleString has the same behavior as toJSON or argumentless toString.
for (const method of ['toString', 'toJSON', 'toLocaleString']) {    
    shouldBe(Temporal.Duration.prototype[method].length, 0);
    shouldThrow(() => Temporal.Duration.prototype[method].call({}), TypeError);

    shouldBe(zero[method](), 'PT0S');
    shouldBe(pos[method](), 'P1Y2M3W4DT5H6M7.00800901S');
    shouldBe(neg[method](), '-P1Y2M3W4DT5H6M7.00800901S');
    
    shouldBe(new Temporal.Duration(1,1,1,1)[method](), 'P1Y1M1W1D');
    shouldBe(new Temporal.Duration(0,0,0,0,1,1,1)[method](), 'PT1H1M1S');
    shouldBe(new Temporal.Duration(0,0,0,0,0,0,0,100)[method](), 'PT0.1S');
    shouldBe(new Temporal.Duration(0,0,0,0,0,0,0,0,100)[method](), 'PT0.0001S');
    shouldBe(new Temporal.Duration(0,0,0,0,0,0,0,0,0,100)[method](), 'PT0.0000001S');
    shouldBe(new Temporal.Duration(0,0,0,0,0,0,1,1001)[method](), 'PT2.001S');
    shouldBe(new Temporal.Duration(0,0,0,0,0,0,0,1,1001)[method](), 'PT0.002001S');
    shouldBe(new Temporal.Duration(0,0,0,0,0,0,0,0,1,1001)[method](), 'PT0.000002001S');
    shouldBe(new Temporal.Duration(0,0,0,0,0,0,1,1001,1001,1001)[method](), 'PT2.002002001S');
}

shouldBe(pos.toString({}), pos.toString());

shouldThrow(() => pos.toString({ smallestUnit: 'foobar' }), RangeError);
for (const unit of ['year', 'month', 'week', 'day', 'hour', 'minute']) {
    shouldThrow(() => pos.toString({ smallestUnit: unit }), RangeError);
    shouldThrow(() => pos.toString({ smallestUnit: `${unit}s` }), RangeError);
}
shouldBe(pos.toString({ smallestUnit: 'seconds' }), 'P1Y2M3W4DT5H6M7S');
shouldBe(pos.toString({ smallestUnit: 'milliseconds' }), 'P1Y2M3W4DT5H6M7.008S');
shouldBe(pos.toString({ smallestUnit: 'microseconds' }), 'P1Y2M3W4DT5H6M7.008009S');
shouldBe(pos.toString({ smallestUnit: 'nanoseconds' }), 'P1Y2M3W4DT5H6M7.008009010S');
for (const unit of ['second', 'millisecond', 'microsecond', 'nanosecond'])
    shouldBe(pos.toString({ smallestUnit: unit }), pos.toString({ smallestUnit: `${unit}s` }));

shouldThrow(() => pos.toString({ fractionalSecondDigits: -1 }), RangeError);
shouldThrow(() => pos.toString({ fractionalSecondDigits: 10 }), RangeError);
shouldThrow(() => pos.toString({ fractionalSecondDigits: 'bogus' }), RangeError);
shouldBe(pos.toString({ fractionalSecondDigits: 0 }), 'P1Y2M3W4DT5H6M7S');
const decimalPart = '008009010';
for (let i = 1; i < 10; i++)
    shouldBe(pos.toString({ fractionalSecondDigits: i }), `P1Y2M3W4DT5H6M7.${decimalPart.slice(0,i)}S`);
shouldBe(pos.toString({ fractionalSecondDigits: 'auto' }), pos.toString());
shouldBe(zero.toString({ fractionalSecondDigits: 2 }), 'PT0.00S');
shouldBe(new Temporal.Duration(1).toString({ fractionalSecondDigits: 2 }), 'P1YT0.00S');

shouldThrow(() => pos.toString({ roundingMode: 'bogus' }), RangeError);
shouldBe(pos.toString({ roundingMode: 'trunc' }), pos.toString());
shouldBe(pos.toString({ fractionalSecondDigits: 7, roundingMode: 'ceil' }), 'P1Y2M3W4DT5H6M7.0080091S');
shouldBe(pos.toString({ fractionalSecondDigits: 2, roundingMode: 'floor' }), 'P1Y2M3W4DT5H6M7.00S');
shouldBe(pos.toString({ fractionalSecondDigits: 2, roundingMode: 'halfExpand' }), 'P1Y2M3W4DT5H6M7.01S');

shouldBe(Temporal.Duration.prototype.valueOf.length, 0);
shouldThrow(() => new Temporal.Duration().valueOf(), TypeError);
