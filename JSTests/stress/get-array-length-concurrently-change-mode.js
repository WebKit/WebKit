//@ requireOptions("--watchdog=10000", "--watchdog-exception-ok")
// This defeats the watchdog on Windows, so we bail after 10 seconds explicitly
const start = Date.now();
function main() {
    runString(`
        function bar(_a) {
          eval(_a);
          arguments.length = 0;
          var array = [
            arguments,
            [0]
          ];
          var result = 0;
          for (var i = 0; i < 1000000; ++i)
            result += array[i % array.length].length;
        }
        bar('bar()');
    `);
    if (Date.now() - start > 10000)
      return
    main();
}
main();
