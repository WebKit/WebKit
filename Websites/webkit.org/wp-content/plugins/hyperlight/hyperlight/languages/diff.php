<?php

class DiffLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'Diff',
            parent::VERSION => '1.0',
            parent::AUTHOR => array(
                parent::NAME => 'Jonathan Davis',
                parent::WEBSITE => 'webkit.org',
                parent::EMAIL => 'jond@webkit.org'
            )
        ));

        $this->setExtensions(array('diff'));
        $this->setCaseInsensitive(true);

        $this->addStates(array(
            'init' => array('removed','added'),
        ));

        $this->addRules(array(
			'added' => new Rule('/^\+/m', '/$/m'),
			'removed' => new Rule('/^\-/m', '/$/m')
        ));

        $this->addMappings(array(
			'added' => 'string',
			'removed' => 'comment'
        ));
    }
}

?>