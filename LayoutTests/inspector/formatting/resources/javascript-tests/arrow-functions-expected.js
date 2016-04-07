x => x;
x => x * x;
x => {x * x};
x => {x * x;};
() => 1;
(x) => x;
(x) => x * x;
(x) => {x * x};
(x) => {x * x;};

x => {
    x *= x;
    x
};

(x) => {
    x *= x;
    x
};

[].sort((a, b) => a - b);
[].sort((a, b) => foo(a, b));
[].reduce((sum, {a, b}) => sum += a + b, 0);

setTimeout(() => {
    1;
    2;
    3
});

foo((a=1, ...rest) => rest[a]);
