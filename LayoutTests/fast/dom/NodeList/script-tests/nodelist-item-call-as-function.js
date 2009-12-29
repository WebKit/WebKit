description('This tests that items in a NodeList can be retrieved directly by calling as a function with an integral index parameter, starting from 0.<br>It means NodeList[0] and NodeList(0) both work.');

var nodeList = document.getElementsByTagName('div');
shouldBe("nodeList[0]", "nodeList(0)");
shouldBe("!nodeList[nodeList.length]", "!nodeList(nodeList.length)");

var successfullyParsed = true;

