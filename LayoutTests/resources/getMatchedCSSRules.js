window.getMatchedCSSRules = (el, css = el.ownerDocument.styleSheets) => 
    [].concat(...[...css].map(s => [...s.cssRules||[]])).filter(r => el.matches(r.selectorText));
