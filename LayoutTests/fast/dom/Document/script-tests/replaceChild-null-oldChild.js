description('Test behavior of Document.replaceChild() when oldChild is null.');

shouldThrow('document.replaceChild(document.firstChild, null)');
