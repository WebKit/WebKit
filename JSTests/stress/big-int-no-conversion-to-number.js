//@ runBigIntEnabled

let message;
try {
  1n + 1;
} catch (error) {
  message = error.message;
}

if (message !== "Conversion from 'BigInt' to 'number' is not allowed.") {
  throw new Error("Error message has changed to something unexpected");
}
