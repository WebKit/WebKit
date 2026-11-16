function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

var regExp = /^\/(?:event\/([^/]+)(?:$()|\/comments$())|map\/([^/]+)\/events$()|static(?:|\/.*)$()|user\/lookup\/(?:email\/([^/]+)$()|username\/([^/]+)$()))/;

function indexOfEmpty(array) {
    return array.indexOf('', 1);
}
noInline(indexOfEmpty);

function indexOfValue(array, value) {
    return array.indexOf(value);
}
noInline(indexOfValue);

function indexOfUndefined(array) {
    return array.indexOf(undefined, 1);
}
noInline(indexOfUndefined);

function indexOfNegativeFrom(array, value) {
    return array.indexOf(value, -3);
}
noInline(indexOfNegativeFrom);

function includesEmpty(array) {
    return array.includes('');
}
noInline(includesEmpty);

function includesValue(array, value) {
    return array.includes(value);
}
noInline(includesValue);

for (var i = 0; i < 1e5; ++i) {
    var match = '/user/lookup/username/hey'.match(regExp);
    shouldBe(indexOfEmpty(match), 10);
    shouldBe(indexOfValue(match, 'hey'), 9);
    shouldBe(indexOfValue(match, 'nonexistent'), -1);
    shouldBe(indexOfUndefined(match), 1);
    shouldBe(indexOfNegativeFrom(match, ''), 10);
    shouldBe(includesEmpty(match), true);
    shouldBe(includesValue(match, 'hey'), true);
    shouldBe(includesValue(match, 'nonexistent'), false);

    var match2 = regExp.exec('/event/42/comments');
    shouldBe(indexOfEmpty(match2), 3);
    shouldBe(indexOfValue(match2, '42'), 1);
    shouldBe(includesValue(match2, '42'), true);
}

var namedRegExp = /(?<year>\d{4})-(?<month>\d{2})(?:-(?<day>\d{2}))?/;
function indexOfOnNamed(array, value) {
    return array.indexOf(value);
}
noInline(indexOfOnNamed);

for (var i = 0; i < 1e5; ++i) {
    var match = '2026-07'.match(namedRegExp);
    shouldBe(indexOfOnNamed(match, '07'), 2);
    shouldBe(indexOfOnNamed(match, undefined), 3);
    shouldBe(indexOfOnNamed(match, '2026-07'), 0);
}
