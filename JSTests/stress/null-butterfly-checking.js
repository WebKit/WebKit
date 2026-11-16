// https://bugs.webkit.org/show_bug.cgi?id=303015

const v3 = {
  p() {
    try {
      this.p();
    } catch (e) {}
    delete this.g;
    this.g = this;
  },
};
v3.p();
v3.p();
