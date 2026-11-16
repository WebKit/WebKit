<?php

require_once('php.php');

class IphpLanguage extends PhpLanguage {
    public function __construct() {
        parent::__construct();
        $this->setExtensions(array()); // Not a whole file, just a fragment.
        $this->removeState('init');
        $this->addStates(array('init' => $this->getState('php')));
    }
}

?>
