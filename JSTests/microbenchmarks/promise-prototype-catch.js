for (let i = 0; i < 1e5; i++) {
  Promise.reject(i).catch(() => { });
}
