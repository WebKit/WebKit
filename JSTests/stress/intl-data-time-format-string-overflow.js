//@ skip if $memoryLimited
(async function () {
    arguments = 'a'.repeat(2147483647 - null);
    new Intl.DateTimeFormat(['de-de'], {
        timeZone: arguments
    });
})();
