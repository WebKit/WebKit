function __f_0() {}
__v_0 = 0
class __c_0 {
  #field = this.init()
  init() {
    if (__v_0 % 2)
      this.anotherField = 0
    return 0
  }
  getField() { this.#field }
}
for (; ; __v_0++) {
  __v_9 = new __c_0
  __f_0(__v_9.getField())
  __v_9.__proto__ = []
  __v_9.getField0
  if (__v_0 == 1000)
    break;
}
