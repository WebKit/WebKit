function filterUIViewTree(treeAsText) {
  return treeAsText
    .split('\n')
    .filter(line =>
      line.includes("scrolling behavior")
      || line.includes("overlay region")
      || line.includes("associated layer")
    )
    .join('\n');
}
