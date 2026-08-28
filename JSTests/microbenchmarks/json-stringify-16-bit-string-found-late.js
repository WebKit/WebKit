const body = { messages: [] };
for (let i = 0; i < 200; ++i)
    body.messages.push({ role: i & 1 ? "assistant" : "user", content: [{ type: "text", text: "abcdefgh".repeat(20) + i }] });
body.messages.push({ role: "user", content: [{ type: "text", text: "\u3042\u3044\u3046" }] });
for (let i = 0; i < 1e4; ++i)
    JSON.stringify(body);
