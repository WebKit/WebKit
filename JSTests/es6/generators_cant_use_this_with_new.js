function test() {

function * generator(){
  yield this.x; yield this.y;
};
try {
  (new generator()).next();
}
catch (e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

