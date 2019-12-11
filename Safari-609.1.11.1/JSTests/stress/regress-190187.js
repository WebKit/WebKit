//@ skip if $memoryLimited or $buildType == "debug"
//@ runDefault
//@ slow!

try {
    var v1 = "AAAAAAAAAAA";
    for(var i = 0; i < 27; i++)
      v1 = v1 + v1;
    var v2;
    var v3 = RegExp.prototype.toString.call({source:v1,flags:v1});
    v3 += v1;
    v2 += v3.localeCompare(v1);
} catch (e) {
    exception = e;
}

if (exception != "Error: Out of memory")
    throw "FAILED";
