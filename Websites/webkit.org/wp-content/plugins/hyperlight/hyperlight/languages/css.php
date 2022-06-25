<?php

class CssLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'CSS',
            parent::VERSION => '0.8',
            parent::AUTHOR => array(
                parent::NAME => 'Konrad Rudolph',
                parent::WEBSITE => 'madrat.net',
                parent::EMAIL => 'konrad_rudolph@madrat.net'
            )
        ));

        $this->setExtensions(array('css'));

        // The following does not conform to the specs but it is necessary
        // else numbers wouldn't be recognized any more.
        $nmstart = '-?[a-z]';
        $nmchar = '[a-z0-9-]';
        $hex = '[0-9a-f]';
        list($string, $strmod) = preg_strip(Rule::STRING);
        $strmod = implode('', $strmod);

        $this->addStates(array(
            'init' => array('comment', 'uri', 'meta', 'id', 'class', 'pseudoclass', 'element', 'block', 'constraint', 'string'),
            'block' => array('comment', 'attribute', 'value'),
            'constraint' => array('identifier', 'string'),
            'value' => array('comment', 'string', 'color', 'number', 'uri', 'identifier', 'important'),
        ));

        $this->addRules(array(
            'attribute' => "/$nmstart$nmchar*/i",
            'value' => new Rule('/:/', '/;|(?=\})/'),
            'comment' => Rule::C_MULTILINECOMMENT,
            'meta' => "/@$nmstart$nmchar*/i",
            'id' => "/#$nmstart$nmchar*/i",
            'class' => "/\.$nmstart$nmchar*/",
            // Pay attention not to match rules such as ::selection!
            'pseudoclass' => "/(?<!:):$nmstart$nmchar*/",
            'element' => "/$nmstart$nmchar*/i",
            'block' => new Rule('/\{/', '/\}/'),
            'constraint' => new Rule('/\[/', '/\]/'),
            'number' => '/[+-]?(?:\d+(\.\d+)?|\d*\.\d+)(%|em|ex|px|pt|in|cm|mm|pc|deg|g?rad|m?s|k?Hz)?/',
            'uri' => "/url\(\s*(?:$string|[^\)]*)\s*\)/$strmod",
            'identifier' => "/$nmstart$nmchar*/i",
            'string' => "/$string/$strmod",
            'color' => "/#$hex{3}(?:$hex{3})?/i",
            'important' => '/!\s*important/',
        ));

        $this->addMappings(array(
            'element' => 'keyword',
            'id' => 'keyword type',
            'class' => 'keyword builtin',
            'pseudoclass' => 'preprocessor',
            'block' => '',
            'constraint' => '',
            'value' => '',
            'color' => 'string',
            'uri' => 'char',
            'meta' => 'keyword',
        ));
    }
}

?>
