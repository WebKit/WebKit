"use strict";

var foo = class {
  constructor() {
    foo();
    try {} catch {}
  }
};
for (var i = 0; i < 1000; i++) {
  try {
    new foo();
  } catch {}
}
