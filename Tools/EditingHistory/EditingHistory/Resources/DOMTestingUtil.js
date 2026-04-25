(() => {
    function subtreeAsString(node, currentDepth = 0) {
        let childNodesAsStrings = Array.from(node.childNodes).map(child => subtreeAsString(child, currentDepth + 1));
        let nodeAsString = node.nodeName;
        if (node.nodeType == Node.ELEMENT_NODE) {
            nodeAsString = `<${nodeAsString}>`
            let attributeDescriptions = [];
            for (let i = 0; i < node.attributes.length; i++) {
                let attribute = node.attributes.item(i);
                attributeDescriptions.push(`${attribute.localName}=${attribute.value}`);
            }
            nodeAsString += `: ${attributeDescriptions.join(", ")}`;
        }

        if (node.nodeType == Node.TEXT_NODE)
            nodeAsString += `: '${node.textContent.replace(/\s/g, " ")}'`;

        return `${"    ".repeat(currentDepth)}${nodeAsString}\n${childNodesAsStrings.join("\n")}`
    }

    window.DOMUtil = {
        subtreeAsString
    };
})();
