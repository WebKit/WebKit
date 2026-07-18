const regExp = /^\/(?:event\/([^/]+)(?:$()|\/comments$())|map\/([^/]+)\/events$()|static(?:|\/.*)$()|user\/lookup\/(?:email\/([^/]+)$()|username\/([^/]+)$()))/;
const match = '/user/lookup/username/hey'.match(regExp);

function test(array) {
    return array.indexOf('', 1);
}
noInline(test);

for (let i = 0; i < 1e6; i++) {
    if (test(match) !== 10)
        throw new Error("bad");
}
