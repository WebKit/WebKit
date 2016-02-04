+1;
-1;
~1;
!1;
!!1;
1 + 1 - 1 * 1 / 1 % 1;
1.2 + 1.2 - 1.2 * 1.2 / 1.2 % 1.2;
(1 + 1 - 1 * 1 / 1 % 1);
(1) + (1) - (1) * (1) / (1) % (1);
+(1);
+s;
-s;
~s;
x = 1;
x = 1e2;
x = 1 + 1;
x = 1e2 + 1e2;
x = x + 1;
x = x = x;
x = x[1] + 1;
x = [1];
x = (1);
x = (s);
x = [1, 1, s];
foo(1, 1, s);
x = [1 + 1 - 1 * 1 / 1 % 1, +1, 1 - 1, -1, ~1, -s, s + 1, s - 1, a[1] + 1, a[1] - 1];
foo(1 + 1 - 1 * 1 / 1 % 1, +1, 1 - 1, -1, ~1, -s, s + 1, s - 1, a[1] + 1, a[1] - 1);
x = [-1 + 1 * 1 / 1 % 1];
x = (-1 + 1 * 1 / 1 % 1);
x = 1 ? +1 : -s;
x = (1 ? +1 : -s);
x = [1 ? -1 : +s];
x = {
    a: -1,
    b: +1,
    c: 1 - 1,
    d: 1,
    s: s
};
// FIXME: Still broken cases. Codemirror treats adjacent operator characters as one operator. For example:
// x=+1 // Codemirror treats "=+" as a single operator.
// x=1?+1:1 // Codemirror treats "?+" as a single operator.
// x=1+-2+1 // Codemirror treats "+-" as a single operator.
// x=1-+2+1 // Codemirror treats "-+" as a single operator.

