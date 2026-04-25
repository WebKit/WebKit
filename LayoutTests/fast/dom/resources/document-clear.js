var doc = "<p>New line 1</p>";
document.open();
document.clear(); // No-op, but should not crash
document.write(doc);
document.close();
