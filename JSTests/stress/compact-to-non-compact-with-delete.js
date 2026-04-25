function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var object = {};
var properties = [];

// compact
for (var i = 0; i < 100; ++i) {
    var key = "Hey" + i;
    object[key] = i;
}
for (var i = 0; i < 16; ++i) {
    var key = "Hey" + i;
    delete object[key];
}
// compact -> non-compact
for (var i = 0; i < 150; ++i) {
    var key = "Hey" + (128 + i);
    object[key] = i;
}
shouldBe(JSON.stringify(Object.keys(object)), `["Hey16","Hey17","Hey18","Hey19","Hey20","Hey21","Hey22","Hey23","Hey24","Hey25","Hey26","Hey27","Hey28","Hey29","Hey30","Hey31","Hey32","Hey33","Hey34","Hey35","Hey36","Hey37","Hey38","Hey39","Hey40","Hey41","Hey42","Hey43","Hey44","Hey45","Hey46","Hey47","Hey48","Hey49","Hey50","Hey51","Hey52","Hey53","Hey54","Hey55","Hey56","Hey57","Hey58","Hey59","Hey60","Hey61","Hey62","Hey63","Hey64","Hey65","Hey66","Hey67","Hey68","Hey69","Hey70","Hey71","Hey72","Hey73","Hey74","Hey75","Hey76","Hey77","Hey78","Hey79","Hey80","Hey81","Hey82","Hey83","Hey84","Hey85","Hey86","Hey87","Hey88","Hey89","Hey90","Hey91","Hey92","Hey93","Hey94","Hey95","Hey96","Hey97","Hey98","Hey99","Hey128","Hey129","Hey130","Hey131","Hey132","Hey133","Hey134","Hey135","Hey136","Hey137","Hey138","Hey139","Hey140","Hey141","Hey142","Hey143","Hey144","Hey145","Hey146","Hey147","Hey148","Hey149","Hey150","Hey151","Hey152","Hey153","Hey154","Hey155","Hey156","Hey157","Hey158","Hey159","Hey160","Hey161","Hey162","Hey163","Hey164","Hey165","Hey166","Hey167","Hey168","Hey169","Hey170","Hey171","Hey172","Hey173","Hey174","Hey175","Hey176","Hey177","Hey178","Hey179","Hey180","Hey181","Hey182","Hey183","Hey184","Hey185","Hey186","Hey187","Hey188","Hey189","Hey190","Hey191","Hey192","Hey193","Hey194","Hey195","Hey196","Hey197","Hey198","Hey199","Hey200","Hey201","Hey202","Hey203","Hey204","Hey205","Hey206","Hey207","Hey208","Hey209","Hey210","Hey211","Hey212","Hey213","Hey214","Hey215","Hey216","Hey217","Hey218","Hey219","Hey220","Hey221","Hey222","Hey223","Hey224","Hey225","Hey226","Hey227","Hey228","Hey229","Hey230","Hey231","Hey232","Hey233","Hey234","Hey235","Hey236","Hey237","Hey238","Hey239","Hey240","Hey241","Hey242","Hey243","Hey244","Hey245","Hey246","Hey247","Hey248","Hey249","Hey250","Hey251","Hey252","Hey253","Hey254","Hey255","Hey256","Hey257","Hey258","Hey259","Hey260","Hey261","Hey262","Hey263","Hey264","Hey265","Hey266","Hey267","Hey268","Hey269","Hey270","Hey271","Hey272","Hey273","Hey274","Hey275","Hey276","Hey277"]`);
