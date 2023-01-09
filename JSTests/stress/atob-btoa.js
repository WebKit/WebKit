function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

let strs = [
    "",
    "3OBQwMDuz29xSLpfDZ3BZpoSmOp5uqpC1AvdUeO4Mj",
    "U5qFQTeWHPtyLAfFpf0DzgNiCXr17LsZxEHRlRoE3S",
    "97tJorct1Pc1IhFuMNCb2XWQ01cVbZF6dQCzcH3QRn",
    "zIom9GE2x1xr6VS6kM8iRoco34an8FVfZ6EvT2kd5EHZ2YxkL91hLhjsmRRsmT6GiOdkSFhOdGJF4GEC42gUosLLNxBmspVl",
    "G8YxPonzURIHs5SOURZnYeASifuSbifqyFoWPexDuxTN3x84Uti00fCUS9DgbqKMIySK4wt9TCeecyr2rD55QDzIlOgmiPUC",
    "rcivMmo7ECcfITpH3uB2FXHfOJH5ILdXoXHbZ4FuzFiPBcTUUfcpSFZlaopWtGSa7YP4Gb9embV2cBsS5vFV6mo7HqCyGAyG",
];

for (let str in strs)
    assert(atob(btoa(str)) === str);

assert(atob(btoa(null)) === "null");
assert(atob(btoa(undefined)) === "undefined");

shouldThrow(() => { btoa("嗨"); }, "Error: Invalid argument for btoa.");
shouldThrow(() => { atob(undefined); }, "Error: Invalid argument for atob.");
