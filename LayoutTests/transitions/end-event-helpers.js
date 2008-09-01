var results = new Array();

function compareEventInfo(e1, e2) {
  // sort by property name then event target id
  // index 0 is the propertyName
  if (e1[0]<e2[0]) return -1;
  if (e1[0]>e2[0]) return +1;
  // index 1 is the target id
  if (e1[1]<e2[1]) return -1;
  if (e1[1]>e2[1]) return +1;
  return 0;
}

function recordEvent(event) {
  results.push([
    event.propertyName,
    event.target.id,
    event.type,
    Math.round(event.elapsedTime * 1000) / 1000 // round off any float errors
    ]);
}

function examineResults(expected) {
  // sort results so events always display in the same order
  results.sort(compareEventInfo);
  var result = '<p>';
  for (var i=0; i < results.length && i < expected.length; ++i) {
    result += "Expected Property: " + expected[i][0] + " ";
    result += "Target: " + expected[i][1] + " ";
    result += "Type: " + expected[i][2] + " ";
    result += "Elapsed Time: " + expected[i][3] + " -- ";
    
    if (expected[i][0] == results[i][0] &&
        expected[i][1] == results[i][1] &&
        expected[i][2] == results[i][2] &&
        expected[i][3] == results[i][3])
      result += "PASS";
    else {
      result += "FAIL -- Received";
      result += "Property: " + results[i][0] + " ";
      result += "Target: " + results[i][1] + " ";
      result += "Type: " + results[i][2] + " ";
      result += "Elapsed Time: " + results[i][3];
      
    }
    result += "<br>";
  }
  result += "</p>";
  
  if (expected.length > results.length) {
    result += "<p>Missing events -- FAIL<br>";
    for (i=results.length; i < expected.length; ++i) {
      result += "Missing Property: " + expected[i][0] + " ";
      result += "Target: " + expected[i][1] + " ";
      result += "Type: " + expected[i][2] + " ";
      result += "Elapsed Time: " + expected[i][3] + "<br>";
    }
    result += "</p>";
  } else if (expected.length < results.length) {
    result += "<p>Unexpected events -- FAIL<br>";
    for (i=expected.length; i < results.length; ++i) {
      result += "Unexpected Property: " + results[i][0] + " ";
      result += "Target: " + results[i][1] + " ";
      result += "Type: " + results[i][2] + " ";
      result += "Elapsed Time: " + results[i][3] + "<br>";
    }
    result += "</p>";
  }
  
  return result;
}

function cleanup()
{
  document.body.removeChild(document.getElementById('container'));
  document.getElementById('result').innerHTML = examineResults(expected);

  if (window.layoutTestController)
      layoutTestController.notifyDone();
}
