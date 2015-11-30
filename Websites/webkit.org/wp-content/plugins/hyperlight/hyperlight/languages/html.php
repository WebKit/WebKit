<?php

require_once('xml.php');

class HtmlLanguage extends XmlLanguage {
    public function __construct() {
        parent::__construct();
        $this->setExtensions(array('html'));

        $this->addMappings(array(
            'attribute' => 'keyword attribute',
            'doctype' => 'doctype'
        ));

    }
}
