// Regression test for bug 178890

if (!/:(?:\w)+\(([']?)((?:\([^\)]+\)|[^\(\)]*){1,2})\1\)/.test(":not('.hs-processed')"))
    throw "/:(?:\w)+\(([']?)((?:\([^\)]+\)|[^\(\)]*){1,2})\1\)/.test(\":not('.hs-processed')\") should succeed, but actually fails";
