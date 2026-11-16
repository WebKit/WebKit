import "./cycle-b.js";
await 0;
throw { someError: "tla-reject" };
