var exception;
try {
    a2 = {};//some method ok//what ever object//Date()
    Object.defineProperty(a2, "length",{get: Int32Array});//Int32Array here wrong,need a function
    new Int32Array(this.a2);
} catch (e) {
    exception = e;
}

if (exception != "TypeError: calling Int32Array constructor without new is invalid")
    throw "Exception not thrown";
