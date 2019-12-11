<?php
class IniLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'INI',
            parent::VERSION => '1.0',
            parent::AUTHOR => array(
                parent::NAME => 'Jonathan Davis',
                parent::WEBSITE => 'webkit.org',
                parent::EMAIL => 'jond@webkit.org'
            )
        ));

        $this->setExtensions(array('ini'));
        $this->setCaseInsensitive(false);


        $this->addStates(array(
            'init' => array('prefix','setting')
		));

        $this->addRules(array(
	        'prefix' => '/\w+\./',
	        'setting' => '/(?<=\.)\w+(?=\s?=)/',
			'comment' => '/#.*/',
        ));

        $this->addMappings(array(
			'prefix' => 'keyword',
			'setting' => 'identifier',
			'value' => 'string'
        ));
    }
}