description("Test basic features of URL canonicalization");

function canonicalize(url) {
  var a = document.createElement("a");
  a.href = url;
  return a.href;
}

cases = [ 
  ["192.168.0.1", "192.168.0.1"],
  ["", ""],
  [".", ""],
  ["192.168.0.1", "192.168.0.1"],
  ["0300.0250.00.01", "192.168.0.1"],
  ["0xC0.0Xa8.0x0.0x1", "192.168.0.1"],
  ["192.168.9.com", ""],
  ["19a.168.0.1", ""],
  ["0308.0250.00.01", ""],
  ["0xCG.0xA8.0x0.0x1", ""],
  ["192", "0.0.0.192"],
];

for (var i = 0; i < cases.length; ++i) {
  shouldBe("canonicalize('http://" + cases[i][0] + "/')",
           "'http://" + cases[i][1] + "/'");
}

var successfullyParsed = true;
