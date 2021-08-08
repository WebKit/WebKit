// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: URI tests
esid: sec-encodeuricomponent-uricomponent
description: Checking RUSSIAN ALPHABET
---*/

//CHECK#1
if ((encodeURIComponent("http://ru.wikipedia.org/wiki/Юникод") !== "http%3A%2F%2Fru.wikipedia.org%2Fwiki%2F%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4") && (encodeURIComponent("http://ru.wikipedia.org/wiki/Юникод") !== "http%3A%2F%2Fru.wikipedia.org%2Fwiki%2F" + "%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4".toLowerCase())) {
  throw new Test262Error('#1: http://ru.wikipedia.org/wiki/Юникод');
}

//CHECK#2
if ((encodeURIComponent("http://ru.wikipedia.org/wiki/Юникод#Ссылки") !== "http%3A%2F%2Fru.wikipedia.org%2Fwiki%2F%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%23%D0%A1%D1%81%D1%8B%D0%BB%D0%BA%D0%B8") && (encodeURIComponent("http://ru.wikipedia.org/wiki/Юникод#Ссылки") !== "http%3A%2F%2Fru.wikipedia.org%2Fwiki%2F" + "%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%23%D0%A1%D1%81%D1%8B%D0%BB%D0%BA%D0%B8".toLowerCase())) {
  throw new Test262Error('#2: http://ru.wikipedia.org/wiki/Юникод#Ссылки');
}

//CHECK#3
if ((encodeURIComponent("http://ru.wikipedia.org/wiki/Юникод#Версии Юникода") !== "http%3A%2F%2Fru.wikipedia.org%2Fwiki%2F%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%23%D0%92%D0%B5%D1%80%D1%81%D0%B8%D0%B8%20%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%D0%B0") && ((encodeURIComponent("http://ru.wikipedia.org/wiki/Юникод%23Версии Юникода") !== "http%3A%2F%2Fru.wikipedia.org%2Fwiki%2F" + "%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4#%D0%92%D0%B5%D1%80%D1%81%D0%B8%D0%B8%20%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%D0%B0".toLowerCase()))) {
  throw new Test262Error('#3: http://ru.wikipedia.org/wiki/Юникод#Версии Юникода');
}
