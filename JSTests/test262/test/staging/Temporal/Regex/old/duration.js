// Copyright (C) 2018 Bloomberg LP. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal-duration-objects
description: Temporal.Duration works as expected
features: [Temporal]
---*/

function test(isoString, components) {
    var {y = 0, mon = 0, w = 0, d = 0, h = 0, min = 0, s = 0, ms = 0, µs = 0, ns = 0} = components;
    var duration = Temporal.Duration.from(isoString);
    assert.sameValue(duration.years, y);
    assert.sameValue(duration.months, mon);
    assert.sameValue(duration.weeks, w);
    assert.sameValue(duration.days, d);
    assert.sameValue(duration.hours, h);
    assert.sameValue(duration.minutes, min);
    assert.sameValue(duration.seconds, s);
    assert.sameValue(duration.milliseconds, ms);
    assert.sameValue(duration.microseconds, µs);
    assert.sameValue(duration.nanoseconds, ns);
}
var day = [
  [
    "",
    {}
  ],
  [
    "1Y",
    { y: 1 }
  ],
  [
    "2M",
    { mon: 2 }
  ],
  [
    "4W",
    { w: 4 }
  ],
  [
    "3D",
    { d: 3 }
  ],
  [
    "1Y2M",
    {
      y: 1,
      mon: 2
    }
  ],
  [
    "1Y3D",
    {
      y: 1,
      d: 3
    }
  ],
  [
    "2M3D",
    {
      mon: 2,
      d: 3
    }
  ],
  [
    "4W3D",
    {
      w: 4,
      d: 3
    }
  ],
  [
    "1Y2M3D",
    {
      y: 1,
      mon: 2,
      d: 3
    }
  ],
  [
    "1Y2M4W3D",
    {
      y: 1,
      mon: 2,
      w: 4,
      d: 3
    }
  ]
];
var times = [
  [
    "",
    {}
  ],
  [
    "4H",
    { h: 4 }
  ],
  [
    "5M",
    { min: 5 }
  ],
  [
    "4H5M",
    {
      h: 4,
      min: 5
    }
  ]
];
var sec = [
  [
    "",
    {}
  ],
  [
    "6S",
    { s: 6 }
  ],
  [
    "7.1S",
    {
      s: 7,
      ms: 100
    }
  ],
  [
    "7.12S",
    {
      s: 7,
      ms: 120
    }
  ],
  [
    "7.123S",
    {
      s: 7,
      ms: 123
    }
  ],
  [
    "8.1234S",
    {
      s: 8,
      ms: 123,
      µs: 400
    }
  ],
  [
    "8.12345S",
    {
      s: 8,
      ms: 123,
      µs: 450
    }
  ],
  [
    "8.123456S",
    {
      s: 8,
      ms: 123,
      µs: 456
    }
  ],
  [
    "9.1234567S",
    {
      s: 9,
      ms: 123,
      µs: 456,
      ns: 700
    }
  ],
  [
    "9.12345678S",
    {
      s: 9,
      ms: 123,
      µs: 456,
      ns: 780
    }
  ],
  [
    "9.123456789S",
    {
      s: 9,
      ms: 123,
      µs: 456,
      ns: 789
    }
  ],
  [
    "0.123S",
    { ms: 123 }
  ],
  [
    "0,123S",
    { ms: 123 }
  ],
  [
    "0.123456S",
    {
      ms: 123,
      µs: 456
    }
  ],
  [
    "0,123456S",
    {
      ms: 123,
      µs: 456
    }
  ],
  [
    "0.123456789S",
    {
      ms: 123,
      µs: 456,
      ns: 789
    }
  ],
  [
    "0,123456789S",
    {
      ms: 123,
      µs: 456,
      ns: 789
    }
  ]
];
var tim = sec.reduce((arr, [s, add]) => arr.concat(times.map(([p, expect]) => [
  `${ p }${ s }`,
  {
    ...expect,
    ...add
  }
])), []).slice(1);
day.slice(1).forEach(([p, expect]) => {
  test(`P${ p }`, expect);
  test(`p${ p }`, expect);
  test(`p${ p.toLowerCase() }`, expect);
});
tim.forEach(([p, expect]) => {
  test(`PT${ p }`, expect);
  test(`Pt${ p }`, expect);
  test(`pt${ p.toLowerCase() }`, expect);
});
for (var [d, dexpect] of day) {
  for (var [t, texpect] of tim) {
    test(`P${ d }T${ t }`, {
      ...dexpect,
      ...texpect
    });
    test(`p${ d }T${ t.toLowerCase() }`, {
      ...dexpect,
      ...texpect
    });
    test(`P${ d.toLowerCase() }t${ t }`, {
      ...dexpect,
      ...texpect
    });
    test(`p${ d.toLowerCase() }t${ t.toLowerCase() }`, {
      ...dexpect,
      ...texpect
    });
  }
}
