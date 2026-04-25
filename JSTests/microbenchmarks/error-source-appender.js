for (let i = 0; i < 1e5; ++i) {
    try {
        undefined.x;
    } catch (err) {
        err.message;
    }
}
