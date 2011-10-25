description(
'This test case checks that months greater than 11 are handled correctly when passed to the Date constructor and the Date.UTC function. ' +
'The ECMA 262 specification says that months &gt; 11 should be treated as month % 12 and the year as year + floor(month / 12).'
);

var expectedUTC = [1104537600000, 1107216000000, 1109635200000, 1112313600000,
                   1114905600000, 1117584000000, 1120176000000, 1122854400000,
                   1125532800000, 1128124800000, 1130803200000, 1133395200000,
                   1136073600000, 1138752000000, 1141171200000, 1143849600000,
                   1146441600000, 1149120000000, 1151712000000, 1154390400000,
                   1157068800000, 1159660800000, 1162339200000, 1164931200000,
                   1167609600000, 1170288000000, 1172707200000, 1175385600000,
                   1177977600000, 1180656000000, 1183248000000, 1185926400000,
                   1188604800000, 1191196800000, 1193875200000, 1196467200000];

for (var i = 0; i < 36; i++) {
    var d = new Date(2005, i, 1);
    shouldBe('d.getFullYear() + "-" + d.getMonth();', '"' + (2005 + Math.floor(i / 12)) + "-" + (i % 12) + '"');
}

for (var i = 0; i < 36; i++) {
  shouldBe('Date.UTC(2005, ' + i + ', 1)', expectedUTC[i].toString());
}
