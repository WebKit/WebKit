var config = config || {};

(function() {

config.kBuilders = [
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

config.kBuildersThatOnlyCompile = [
    'Webkit Win Builder',
    'Webkit Win Builder (dbg)',
    // FIXME: Where is the Linux Builder?
    'Webkit Mac Builder',
    'Webkit Mac Builder (dbg)',
    'Win Builder',
    'Mac Clang Builder (dbg)',
];

config.kTracURL = 'http://trac.webkit.org';

config.kRevisionAttr = 'data-revision';
config.kTestNameAttr = 'data-test-name';
config.kBuilderNameAttr = 'data-builder-name';
config.kFailureCountAttr = 'data-failure-count';
config.kFailureTypesAttr = 'data-failure-types';
config.kAlertTypeAttr = 'data-alert-type';

var kTenMinutesInMilliseconds = 10 * 60 * 1000;
config.kUpdateFrequency = kTenMinutesInMilliseconds;

})();
