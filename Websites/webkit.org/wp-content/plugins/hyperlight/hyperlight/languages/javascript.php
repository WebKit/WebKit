<?php

class JavaScriptLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'JavaScript',
            parent::VERSION => '1.0',
            parent::AUTHOR => array(
                parent::NAME => 'Jonathan Davis',
                parent::WEBSITE => 'webkit.org',
                parent::EMAIL => 'jond@webkit.org'
            )
        ));

        $this->setExtensions(array('js','json','html','php'));

        $this->addStates(array(
            'init' => array(
                'string', 'char', 'number', 'comment',
                'keyword' => array('', 'type', 'modifier','control','literal', 'operator'),
                'identifier',
                'operator'
            )
        ));

        $this->addRules(array(
            'whitespace' => RULE::ALL_WHITESPACE,
            'operator' => '/[!|%|&|\*|\-\-|\-|\+\+|\+|~|===|==|=|!=|!==|<=|>=|<<=|>>=|>>>=|<>|<|>|!|&&|\|\||\?\:|\*=|\/=|%=|\+=|\-=|&=|\^=|:]/',
            'string' => Rule::C_DOUBLEQUOTESTRING,
            'char' => Rule::C_SINGLEQUOTESTRING,
            'number' => Rule::C_NUMBER,
            'comment' => Rule::C_COMMENT,
            'keyword' => array(
                array(
                    'super','this','arguments','prototype','constructor'
                ),
                'type' => array(
                    'boolean', 'byte', 'char', 'class', 'const', 'double', 'enum', 'float', 'function', 'int', 'interface', 'let', 'long', 'short', 'var', 'void'
                ),
                'modifier' => array(
                    'const','export','extends','final','implements','native','private','protected','public','static','synchronized','throws','transient','volatile'
                ),
                'control' => array(
                    'break','case','catch','continue','default','do','else','finally','for','goto','if','import','package','return','switch','throw','try','while'
                ),
                'literal' => array(
                    'false', 'this', 'true', 'null', 'undefined', 'NaN'
                ),
                'operator' => array(
                    'delete', 'in', 'instanceof', 'new', 'of', 'typeof', 'void', 'with'
                ),
            ),
            'identifier' => Rule::C_IDENTIFIER,
        ));

        $this->addMappings(array(
        ));
    }
}