function log(message)
{
    document.getElementById("result").innerHTML += message + "<br>";
}

function shouldBe(a, b)
{
  aValue = eval(a);
  bValue = eval(b);
  if (aValue == bValue)
     log('PASS: ' + a + ' equaled ' + b);
  else
     log('FAILED: ' + a + '(' + aValue + ') did not equal ' + b + '(' + bValue + ')');
}
