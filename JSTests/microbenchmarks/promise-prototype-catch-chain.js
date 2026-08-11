const p = Promise.reject(333);
for (let i = 0; i < 1e6; i++) {
    p.catch(() => { }).catch(() => { }).catch(() => { });
}
