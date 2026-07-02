function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const options = {
    calendar: 'buddhist',
    caseFirst: 'lower',
    collation: 'eor日本語'.substring(0, 3),
    hourCycle: 'h11',
    numeric: false,
    numberingSystem: 'thai',
    language: 'ja',
    script: 'Hant',
    region: 'KR'
};
const expected = 'ja-Hant-KR-u-ca-buddhist-co-eor-hc-h11-kf-lower-kn-false-nu-thai';
shouldBe(new Intl.Locale('en', options).toString(), expected);
