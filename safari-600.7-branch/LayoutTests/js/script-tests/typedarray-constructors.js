description(
'This test case tests the various typed array and related constructors. ' +
'In particular, makes sure that you use the "new" keyword when using the constructors.'
);

shouldThrow("Int8Array()");
shouldNotThrow("new Int8Array()");

shouldThrow("Int16Array()");
shouldNotThrow("new Int16Array()");

shouldThrow("Int32Array()");
shouldNotThrow("new Int32Array()");

shouldThrow("Uint8Array()");
shouldNotThrow("new Uint8Array()");

shouldThrow("Uint16Array()");
shouldNotThrow("new Uint16Array()");

shouldThrow("Uint32Array()");
shouldNotThrow("new Uint32Array()");

shouldThrow("Uint8ClampedArray()");
shouldNotThrow("new Uint8ClampedArray()");

shouldThrow("Float32Array()");
shouldNotThrow("new Float32Array()");

shouldThrow("Float64Array()");
shouldNotThrow("new Float64Array()");

shouldThrow("DataView(new ArrayBuffer())");
shouldNotThrow("new DataView(new ArrayBuffer())");

