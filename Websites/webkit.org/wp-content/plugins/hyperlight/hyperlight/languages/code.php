<?php

class CodeLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'Code',
            parent::VERSION => '1.0',
            parent::AUTHOR => array(
                parent::NAME => 'Jonathan Davis',
                parent::WEBSITE => 'webkit.org',
                parent::EMAIL => 'jond@webkit.org'
            )
        ));

        $this->setExtensions(array(''));

        $this->addStates(array(
            'init' => array('comment'),
        ));

	    $this->addRules(array(
			'comment' => '/#.*/'
	    ));

    }
}

?>