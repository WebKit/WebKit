<?php

class SyntaxLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'Syntax',
            parent::VERSION => '1.0',
            parent::AUTHOR => array(
                parent::NAME => 'Jonathan Davis',
                parent::WEBSITE => 'webkit.org',
                parent::EMAIL => 'jond@webkit.org'
            )
        ));

        $this->setExtensions(array());

        $this->addStates(array(
            'init' => array('prototype','variable','optional','return','params','prop','newlines'),
			'prototype' => array('name','interface'),
			'interface' => array('variable','optional'),
			'params' => array('param'),
			'param' => array('key','datatype','variable','notice'),
			'return' => array('key','returntype','notice')
        ));

	    $this->addRules(array(
			'prototype' => new Rule('/\b(?=\w+\s?\()/','/\n/'),
			'newlines' => '/\n\n/',
			'name' => '/\w+?(?=\(.*?\))/',
			'interface' => new Rule('/\(/','/\)/'),
			'variable' => '/\$\w+/',
			'params' => new Rule('/\n(?=@(param))/','/((?=@return)|\n{2,})/'),
			'param' => new Rule('/(?=@param)/','/\n/'),
			'return' => new Rule('/(?=@return)/','/\n/'),
			'key' => '/@\w+\s/',
            'datatype' => '/\w+(?=\s\$)/',
			'returntype' => '/(?<=@return\s)\w+/',
			'notice' => '/\(.*?\)/',
            'optional' => new Rule('/\[/','/\]/')
	    ));

	    $this->addMappings(array(
			'name' => 'keyword builtin',
			'variable' => 'identifier',
			'optional' => 'string',
			'notice' => 'string',
			'type' => 'keyword type',
			'prop' => 'hidden',
			'keytype' => 'hidden',
			'datatype' => 'keyword literal',
			'returntype' => 'keyword literal'
	    ));

    }
}

?>