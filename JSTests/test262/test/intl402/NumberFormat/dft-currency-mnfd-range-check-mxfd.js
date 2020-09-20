// Copyright 2017 the V8 project authors. All rights reserved.
// Copyright 2020 Apple Inc. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/*---
esid: sec-setnfdigitoptions
description: >
    When a currency is used in Intl.NumberFormat and minimumFractionDigits is
    not provided, maximumFractionDigits should be set as provided if it is larger
    than currencyDigits.
---*/

assert.throws(RangeError, () => {
    new Intl.NumberFormat('en', {
        style: 'currency',
        currency: 'USD',
        maximumFractionDigits: 1
    });
});

assert.sameValue((new Intl.NumberFormat('en', {
    style: 'currency',
    currency: 'USD',
    maximumFractionDigits: 2
})).resolvedOptions().maximumFractionDigits, 2);

assert.sameValue((new Intl.NumberFormat('en', {
    style: 'currency',
    currency: 'USD',
    maximumFractionDigits: 3
})).resolvedOptions().maximumFractionDigits, 3);

assert.sameValue((new Intl.NumberFormat('en', {
    style: 'currency',
    currency: 'USD',
    maximumFractionDigits: 20
})).resolvedOptions().maximumFractionDigits, 20);

assert.throws(RangeError, () => {
    new Intl.NumberFormat('en', {
        style: 'currency',
        currency: 'USD',
        maximumFractionDigits: 21
    })
});

assert.throws(RangeError, () => {
    new Intl.NumberFormat('en', {
        style: 'currency',
        currency: 'CLF',
        maximumFractionDigits: 3
    })
});

assert.sameValue((new Intl.NumberFormat('en', {
    style: 'currency',
    currency: 'CLF',
    maximumFractionDigits: 4
})).resolvedOptions().maximumFractionDigits, 4);

assert.sameValue((new Intl.NumberFormat('en', {
    style: 'currency',
    currency: 'CLF',
    maximumFractionDigits: 5
})).resolvedOptions().maximumFractionDigits, 5);

assert.sameValue((new Intl.NumberFormat('en', {
    style: 'currency',
    currency: 'CLF',
    maximumFractionDigits: 20
})).resolvedOptions().maximumFractionDigits, 20);

assert.throws(RangeError, () => {
    new Intl.NumberFormat('en', {
        style: 'currency',
        currency: 'CLF',
        maximumFractionDigits: 21
    })
});
