const regExp = /^\/(?:event\/([^/]+)(?:$()|\/comments$())|map\/([^/]+)\/events$()|static(?:|\/.*)$()|user\/lookup\/(?:email\/([^/]+)$()|username\/([^/]+)$()))/;
const match = '/user/lookup/username/hey'.match(regExp);

function test(array) {
    return array.includes('hey');
}
noInline(test);

for (let i = 0; i < 1e6; i++) {
    if (!test(match))
        throw new Error("bad");
}
