function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

shouldBe(Intl.NumberFormat.prototype.formatToParts instanceof Function, true);
shouldBe(Intl.NumberFormat.prototype.formatToParts.length, 1);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'formatToParts').writable, true);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'formatToParts').enumerable, false);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'formatToParts').configurable, true);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'formatToParts').get, undefined);
shouldBe(Object.getOwnPropertyDescriptor(Intl.NumberFormat.prototype, 'formatToParts').set, undefined);

// Handles non-finite and non-number
shouldBe(
    JSON.stringify(Intl.NumberFormat('en').formatToParts(-Infinity)),
    JSON.stringify([{ type: 'minusSign', value: '-' }, { type: 'infinity', value: '∞' }])
);
shouldBe(
    JSON.stringify(Intl.NumberFormat('en').formatToParts('one')),
    JSON.stringify([{ type: 'nan', value: 'NaN' }])
);

// Handles percents
shouldBe(
    JSON.stringify(Intl.NumberFormat('en-US', { style: 'percent' }).formatToParts(1)),
    JSON.stringify([{ type: 'integer', value: '100' }, { type: 'percentSign', value: '%' }])
);

// Handles locale, currency, and number system
shouldBe(
    JSON.stringify(
        Intl.NumberFormat('pt-BR-u-nu-fullwide', {
            minimumFractionDigits: '3', style: 'currency', currency: 'USD', currencyDisplay: 'name'
        }).formatToParts(21069933563725.1)
    ),
    JSON.stringify([
        { type: 'integer', value: '２１' },
        { type: 'group', value: '.' },
        { type: 'integer', value: '０６９' },
        { type: 'group', value: '.' },
        { type: 'integer', value: '９３３' },
        { type: 'group', value: '.' },
        { type: 'integer', value: '５６３' },
        { type: 'group', value: '.' },
        { type: 'integer', value: '７２５' },
        { type: 'decimal', value: ',' },
        { type: 'fraction', value: '１００' },
        { type: 'literal', value: ' ' },
        { type: 'currency', value: 'Dólares americanos' },
    ])
);

shouldBe(+Intl.NumberFormat('en-US', { useGrouping: false }).formatToParts(Number.MAX_SAFE_INTEGER)[0].value, Number.MAX_SAFE_INTEGER);

// Exceed the 32 character default buffer size
shouldBe(Intl.NumberFormat('en-US', { useGrouping: false }).formatToParts(Number.MAX_VALUE).length, 1);
shouldBe(Intl.NumberFormat('en-US', { useGrouping: false }).formatToParts(Number.MAX_VALUE)[0].value.length, 309);
shouldBe(Intl.NumberFormat('en-US').formatToParts(Number.MAX_VALUE).length, 205);

shouldBe(
    JSON.stringify(
        Intl.NumberFormat('en-US').formatToParts(4000n)
    ),
    JSON.stringify([
        {"type":"integer","value":"4"},
        {"type":"group","value":","},
        {"type":"integer","value":"000"}
    ]));

shouldBe(
    JSON.stringify(
        Intl.NumberFormat('en-US').formatToParts(-4000n)
    ),
    JSON.stringify([
        {"type":"minusSign","value":"-"},
        {"type":"integer","value":"4"},
        {"type":"group","value":","},
        {"type":"integer","value":"000"}
    ]));

shouldBe(
    JSON.stringify(
        Intl.NumberFormat('en-US').formatToParts("-4000")
    ),
    JSON.stringify([
        {"type":"minusSign","value":"-"},
        {"type":"integer","value":"4"},
        {"type":"group","value":","},
        {"type":"integer","value":"000"}
    ]));
