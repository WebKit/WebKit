try {
    const x = Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get;
    x();
    const sticky = 0;
} catch { }
