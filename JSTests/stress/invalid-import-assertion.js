import("./resources/empty.js", { with: { type: "<invalid>" } }).then($vm.abort, function () {});
