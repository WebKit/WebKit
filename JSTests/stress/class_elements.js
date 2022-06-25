function test() {

let log = [];
function effect(desc) { log.push(desc); return desc; }

class C {
  [effect("instance#1")]() {}
  static [effect("static#2")]() {}
  get [effect("instanceGetter#3")]() {}
  static get [effect("staticGetter#4")]() {}
  set [effect("instanceSetter#5")](v) {}
  static [effect("staticSetter#6")](v) {}
}

return log[0] === "instance#1" &&
       log[1] === "static#2" &&
       log[2] === "instanceGetter#3" &&
       log[3] === "staticGetter#4" &&
       log[4] === "instanceSetter#5" &&
       log[5] === "staticSetter#6";
}

if (!test())
    throw new Error("Test failed");