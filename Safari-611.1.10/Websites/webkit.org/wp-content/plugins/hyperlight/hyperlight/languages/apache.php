<?php

class ApacheLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'Apache',
            parent::VERSION => '1.0',
            parent::AUTHOR => array(
                parent::NAME => 'Jonathan Davis',
                parent::WEBSITE => 'webkit.org',
                parent::EMAIL => 'jond@webkit.org'
            )
        ));

        $this->setExtensions(array('htaccess','conf'));
        $this->setCaseInsensitive(false);

        $this->addStates(array(
            'init' => array('extension','mimetype','comment','regex','identifier','operator','path',
		        				'keyword' => array('', 'type')),
        ));

        $this->addRules(array(
	        'keyword' => array(
				array('AccessFileName','AllowOverride','Options','AddHandler','SetHandler',
					'AuthType','AuthName','AuthUserFile','AuthGroupFile','Require','IfModule',
					'RewriteRule','RewriteCond'
				),
				'type' => array(
					'mod_fastcgi.c'
                ),
			),
			'regex' => new Rule('/\^/', '/\$/'),
			'path' => '/\s\/(.+?)\s/',
			'operator' => '/(\[.+\]|!-\w)\n/',
			'identifier' => '/%{.+}/',
			'extension' => '/(?<=\s)\.\w+/',
			'mimetype' => '/[\w\.\-]+\/[\w\.\-]+/',
			'comment' => '/#.*/',
        ));

        $this->addMappings(array(
			'keyword' => 'keyword builtin',
			'path' => 'string',
			'extension' => 'identifier',
			'mimetype' => 'string'
        ));
    }
}

?>
