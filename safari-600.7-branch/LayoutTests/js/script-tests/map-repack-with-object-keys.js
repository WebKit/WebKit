description("Tests to make sure we correctly repack a Map with object keys");

var map = new Map();
function Obj(n) { this.n = n; }

map.set(new Obj(0), []);
map.set(new Obj(1), []);
map.set(new Obj(2), []);
map.set(new Obj(3), []);
map.set(new Obj(4), []);
map.set(new Obj(5), []);
map.set(new Obj(6), []);
map.set(new Obj(7), []);

var newObject1 = new Obj(8);
var newObject2 = new Obj(9);
map.set(newObject1, []);
map.set(newObject2, []);
map.delete(newObject1);
map.delete(newObject2);
map.set(newObject1, []);
map.set(newObject2, []);
map.delete(newObject1);
map.delete(newObject2);

map.set(newObject1, []);
shouldBeTrue("Array.isArray(map.get(newObject1))");

map.set(newObject2, []);
shouldBeTrue("Array.isArray(map.get(newObject1))"); // ensure pre-existing value is still good.
