if ("" + /(?<=\u{10000}\1*(a))b/u.exec("\u{10000}ab") !== "b,a")
    throw 'Expected ["b", "a"]';
