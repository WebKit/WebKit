// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: URI tests
esid: sec-encodeuri-uri
description: Checking RUSSIAN ALPHABET
---*/

//CHECK#1
if ((encodeURI("http://ru.wikipedia.org/wiki/Юникод") !== "http://ru.wikipedia.org/wiki/%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4") && (encodeURI("http://ru.wikipedia.org/wiki/Юникод") !== "http://ru.wikipedia.org/wiki/" + "%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4".toLowerCase())) {
  throw new Test262Error('#1: http://ru.wikipedia.org/wiki/Юникод');
}

//CHECK#2
if ((encodeURI("http://ru.wikipedia.org/wiki/Юникод#Ссылки") !== "http://ru.wikipedia.org/wiki/%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4#%D0%A1%D1%81%D1%8B%D0%BB%D0%BA%D0%B8") && (encodeURI("http://ru.wikipedia.org/wiki/Юникод#Ссылки") !== "http://ru.wikipedia.org/wiki/" + "%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4#%D0%A1%D1%81%D1%8B%D0%BB%D0%BA%D0%B8".toLowerCase())) {
  throw new Test262Error('#2: http://ru.wikipedia.org/wiki/Юникод#Ссылки');
}

//CHECK#3
if ((encodeURI("http://ru.wikipedia.org/wiki/Юникод#Версии Юникода") !== "http://ru.wikipedia.org/wiki/%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4#%D0%92%D0%B5%D1%80%D1%81%D0%B8%D0%B8%20%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%D0%B0") && ((encodeURI("http://ru.wikipedia.org/wiki/Юникод#Версии Юникода") !== "http://ru.wikipedia.org/wiki/" + "%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4#%D0%92%D0%B5%D1%80%D1%81%D0%B8%D0%B8%20%D0%AE%D0%BD%D0%B8%D0%BA%D0%BE%D0%B4%D0%B0".toLowerCase()))) {
  throw new Test262Error('#3: http://ru.wikipedia.org/wiki/Юникод#Версии Юникода');
}
