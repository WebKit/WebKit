var willChangeValues = [
    { 'property' : 'auto', 'stacking' : false },
    { 'property' : 'background', 'stacking' : false },
    { 'property' : 'will-change', 'stacking' : false },
    { 'property' : '-webkit-backface-visibility', 'stacking' : false },

    { 'property' : 'clip-path', 'stacking' : true },
    { 'property' : 'mask', 'stacking' : true },
    { 'property' : 'opacity', 'stacking' : true },
    { 'property' : 'position', 'stacking' : true },
    { 'property' : 'z-index', 'stacking' : true },
    { 'property' : 'mix-blend-mode', 'stacking' : true },
    { 'property' : 'isolation', 'stacking' : true },
    { 'property' : 'perspective', 'stacking' : true },
    { 'property' : 'transform', 'stacking' : true },
    { 'property' : 'transform-style', 'stacking' : true },

    { 'property' : '-webkit-filter', 'stacking' : true },
    { 'property' : '-webkit-clip-path', 'stacking' : true },
    { 'property' : '-webkit-box-reflect', 'stacking' : true },
    { 'property' : '-webkit-backdrop-filter', 'stacking' : true },
    { 'property' : '-webkit-mask', 'stacking' : true },
    { 'property' : '-webkit-mask-image', 'stacking' : true },
    { 'property' : '-webkit-mask-box-image', 'stacking' : true },
    { 'property' : '-webkit-transform', 'stacking' : true },
    { 'property' : '-webkit-transform-style', 'stacking' : true },
    { 'property' : '-webkit-flow-from', 'stacking' : true },
];

function makeStackingElement(stackingProperty, value)
{
    var container = document.createElement('div');
    container.className = 'container';

    var potentialStackingContext = document.createElement('div');
    potentialStackingContext.className = 'potential-stacking-context';
    potentialStackingContext.style[stackingProperty] = value;

    var child = document.createElement('div');
    child.className = 'child';
    potentialStackingContext.appendChild(child);

    container.appendChild(potentialStackingContext);

    var interposer = document.createElement('div');
    interposer.className = 'interposer';
    container.appendChild(interposer);
    
    document.body.appendChild(container);
}
