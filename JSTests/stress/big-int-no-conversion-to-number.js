let message;
try {
  1n + 1;
} catch (error) {
  message = error.message;
}

if (message !== "Invalid mix of BigInt and other type in addition.") {
  throw new Error("Error message has changed to something unexpected");
}
