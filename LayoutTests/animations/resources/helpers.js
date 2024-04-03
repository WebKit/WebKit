'use strict';

function matricesApproxEqual(actualMatrix, expectedMatrix, epsilon) {
  const actualNumbers = actualMatrix.split(/[\(\) ,]/);
  const expectedNumbers = expectedMatrix.split(/[\(\) ,]/);
  if (actualNumbers.length !== expectedNumbers.length) {
    return false;
  }
  for (var i = 0; i < actualNumbers.length; i++) {
    if (actualNumbers[i] !== expectedNumbers[i] &&
        Math.abs(Number(actualNumbers[i]) - Number(expectedNumbers[i])) > epsilon) {
      return false;
    }
  }
  return true;
}
