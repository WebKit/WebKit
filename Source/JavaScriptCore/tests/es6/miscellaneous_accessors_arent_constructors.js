function test() {

try {
  new (Object.getOwnPropertyDescriptor({get a(){}}, 'a')).get;
} catch(e) {
  return true;
}
      
}

if (!test())
    throw new Error("Test failed");

