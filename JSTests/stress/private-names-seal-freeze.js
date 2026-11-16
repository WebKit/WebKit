class C {
  #field;
  setField(v) {
    this.#field = v;
  }
}

for (let i = 0; i < 10000; i++) {
  let c = new C();
  Object.seal(c);
  c.setField(i);
}

for (let i = 0; i < 10000; i++) {
  let c = new C();
  Object.freeze(c);
  c.setField(i);
}
