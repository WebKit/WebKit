function test(str, start, end) {
    return String.prototype.substring.call(str, start, end);
}

var str = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium totam rem aperiam.";

for (var i = 0; i < 1e6; ++i) {
    var len = str.length;
    var startIndex = (i * 7) % len;
    var endIndex = startIndex + 1;
    test(str, startIndex, endIndex);
}

