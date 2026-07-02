<?php
require_once('javascript.php');

class BlocklistLanguage extends JavascriptLanguage {
    
    public function __construct() {
        parent::__construct();
        $this->setExtensions(array('json-bl'));

        $this->removeState('init');
        $this->addStates(array(
            'init' => array(
	            'trigger', 'action', 'value', 'char', 'number', 'comment',
	            'keyword' => array('', 'type', 'modifier','control','literal', 'operator'),
	            'identifier',
	            'operator'
	        )
        ));
        
        $this->addRules(array(
            'trigger' => array('url-filter-is-case-sensitive', 'url-filter',  'resource-type', 'load-type', 'if-domain', 'unless-domain'),
            'action' => array('type', 'selector'),
            'value' => new Rule('/(?<=:\s)"/', '/"(\n|}|,)/'),
            'identifier' => array('trigger', 'action')
        ));

        $this->addMappings(array(
            'trigger' => 'keyword',
            'action' => 'keyword',
            'value' => 'string'
        ));
    }
}