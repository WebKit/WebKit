Promise.resolve().then(0, makeMasquerader());
Promise.resolve().then(makeMasquerader(), 0);
Promise.resolve().then(makeMasquerader(), makeMasquerader());
