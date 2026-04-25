// Babel plugin that adds CACHE_BUST_COMMENT to every function body.
const CACHE_BUST_COMMENT = "ThouShaltNotCache";


module.exports = function({ types: t }) {
  return {
    visitor: {
      Function(path) {
          const bodyPath = path.get("body");
          // Handle arrow functions: () => "value"
          // Convert them to block statements: () => { return "value"; }
          if (!bodyPath.isBlockStatement()) {
            const newBody = t.blockStatement([t.returnStatement(bodyPath.node)]);
            path.set("body", newBody);
          }

          // Handle empty function bodies: function foo() {}
          // Add an empty statement so we have a first node to attach the comment to.
          if (path.get("body.body").length === 0) {
            path.get("body").pushContainer("body", t.emptyStatement());
          }

          const firstNode = path.node.body.body[0];
          t.addComment(firstNode, "leading", CACHE_BUST_COMMENT);

      }
    },
  };
};
