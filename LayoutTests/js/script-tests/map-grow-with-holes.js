description("Tests to make sure we correctly handle map compaction when growing a Map with holes");

/*
 * This test case clobbers the Map with a number of new properties with periodic deletes
 * creating holes in the backing store.
 *
 * When the Map requires more space we trigger an expand and compact, which then
 * rebuilds the key->index hashmap.  This means that we need to ensure that the
 * newly added key is not in hashmap during the compaction.
 *
 * We duplicate the test as string keyed entries use a distinct hashmap
 * for key->index.
 */

var map = new Map();
var flow = {};

map.clear();
map.clear();
map.has(81);
map.set(81, flow);
map.has(83);
map.set(83, flow);
map.has(85);
map.set(85, flow);
map.has(87);
map.set(87, flow);
map.has(89);
map.set(89, flow);
map.has(91);
map.set(91, flow);
map.has(81);
map.get(81);
map.delete(81);
map.has(82);
map.has(83);
map.get(83);
map.delete(83);
map.has(84);
map.has(85);
map.get(85);
map.delete(85);
map.has(86);
map.has(87);
map.get(87);
map.delete(87);
map.has(88);
map.has(89);
map.get(89);
map.delete(89);
map.has(90);
map.has(91);
map.get(91);
map.delete(91);
map.has(92);
map.has(93);
map.set(93, flow);
map.has(95);
map.set(95, flow);
map.has(97);
map.set(97, flow);
map.has(99);
map.set(99, flow);
map.has(101);
map.set(101, flow);
map.has(103);
map.set(103, flow);
map.has(105);
map.set(105, flow);
map.has(93);
map.get(93);
map.delete(93);
map.has(94);
map.has(95);
map.get(95);
map.delete(95);
map.has(96);
map.has(97);
map.get(97);


map = new Map();
var flow = {};

map.clear();
map.clear();
map.has("81");
map.set("81", flow);
map.has("83");
map.set("83", flow);
map.has("85");
map.set("85", flow);
map.has("87");
map.set("87", flow);
map.has("89");
map.set("89", flow);
map.has("91");
map.set("91", flow);
map.has("81");
map.get("81");
map.delete("81");
map.has("82");
map.has("83");
map.get("83");
map.delete("83");
map.has("84");
map.has("85");
map.get("85");
map.delete("85");
map.has("86");
map.has("87");
map.get("87");
map.delete("87");
map.has("88");
map.has("89");
map.get("89");
map.delete("89");
map.has("90");
map.has("91");
map.get("91");
map.delete("91");
map.has("92");
map.has("93");
map.set("93", flow);
map.has("95");
map.set("95", flow);
map.has("97");
map.set("97", flow);
map.has("99");
map.set("99", flow);
map.has("101");
map.set("101", flow);
map.has("103");
map.set("103", flow);
map.has("105");
map.set("105", flow);
map.has("93");
map.get("93");
map.delete("93");
map.has("94");
map.has("95");
map.get("95");
map.delete("95");
map.has("96");
map.has("97");
map.get("97");


