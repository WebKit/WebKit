function hypot2(arg1, arg2) {
    return Math.hypot(arg1, arg2);
}

function hypot3(arg1, arg2, arg3) {
    return Math.hypot(arg1, arg2, arg3);
}
noInline(hypot2);
noInline(hypot3);

for (var i = 0; i < testLoopCount; ++i) {
    hypot2(3, 4);
    hypot2(9, 12);
    hypot3(400.2, -3.4, 3);
    hypot3(-24.3, -400.2, -0.4);
}
