const result = /\B|c./u.exec("a\ud800\udc00")[0][0];
if (result !== undefined)
    throw "Expected undefined, got " + result;

