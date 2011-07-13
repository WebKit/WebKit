var config = config || {};

(function() {

config.builders = [
  'Webkit Win',
  'Webkit Vista',
  // 'Webkit Win7',
  'Webkit Win (dbg)(1)',
  'Webkit Win (dbg)(2)',
  'Webkit Linux',
  'Webkit Linux 32',
  'Webkit Linux (dbg)(1)',
  'Webkit Linux (dbg)(2)',
  'Webkit Mac10.5',
  'Webkit Mac10.5 (dbg)(1)',
  'Webkit Mac10.5 (dbg)(2)',
  'Webkit Mac10.6',
  'Webkit Mac10.6 (dbg)',
];

config.kTestNameAttr = 'data-test-name';
config.kBuilderNameAttr = 'data-builder-name';
config.kFailureCountAttr = 'data-failure-count';
config.kFailureTypesAttr = 'data-failure-types';

})();
