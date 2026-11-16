var cities = ["a", "b", "c", "d", "e", "f", "g", "h"];
var state = { id: 1, name: "x", age: 3, city: "z", zip: 0, flag: true, score: 1.5, tag: "t" };
for (var i = 0; i < 1e4; ++i)
    state = Object.assign({}, state, { city: cities[i & 7], zip: i });
