
function visualViewportDescription()
{
	const viewport = window.visualViewport;
	return `left: ${Math.round(viewport.pageLeft)} top ${Math.round(viewport.pageTop)} width ${Math.round(viewport.width)} height ${Math.round(viewport.height)} scale ${viewport.scale.toFixed(2)}`;
}
