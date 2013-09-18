description(
"This tests that get_by_pname doesn't incorrectly think that an empty object can cache its prototype's properties."
);

var foo = function (C, A) {
    for(var B in (A||{})) {
        C[B]=A[B];
    }
    return C;
}

var protos = [];
for (var i = 0; i < 256; i++) {
    var proto = Object.create(null);
    protos.push(proto);
    proto.aa = 1;
    proto.ab = 1;
    proto.ac = 1;
    proto.ad = 1;
    proto.ae = 1;
    proto.af = 1;
    proto.ag = 1;
    proto.ah = 1;
    proto.ai = 1;
    proto.aj = 1;
    proto.ak = 1;
    proto.al = 1;
    proto.am = 1;
    proto.an = 1;
    proto.ao = 1;
    proto.ap = 1;
    proto.aq = 1;
    proto.ar = 1;
    proto.as = 1;
    proto.at = 1;
    proto.au = 1;
    proto.av = 1;
    proto.aw = 1;
    proto.ax = 1;
    proto.ay = 1;
    proto.az = 1;
    proto.ba = 1;
    proto.bb = 1;
    proto.bc = 1;
    proto.bd = 1;
    proto.be = 1;
    proto.bf = 1;
    var weirdObject = Object.create(proto);
    var result = foo({}, weirdObject);
    for (var p in result) {
        if (result[p] !== result["" + p])
            shouldBe("result[p]", "result[\"\" + p]");
    }
}
