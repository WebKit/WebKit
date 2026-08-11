<?php

class XmlLanguage extends HyperLanguage {
    public function __construct() {
        $this->setInfo(array(
            parent::NAME => 'XML',
            parent::VERSION => '0.3',
            parent::AUTHOR => array(
                parent::NAME => 'Konrad Rudolph',
                parent::WEBSITE => 'madrat.net',
                parent::EMAIL => 'konrad_rudolph@madrat.net'
            )
        ));

        $this->setExtensions(array('xml', 'xsl', 'xslt', 'xsd', 'manifest'));

        $inline = array('entity');
        $common = array('tagname', 'attribute', 'value' => array('double', 'single'));

        $this->addStates(array(
            'init' => array_merge(array('doctype', 'comment', 'cdata', 'tag'), $inline),
            'tag' => array_merge(array('preprocessor', 'meta'), $common),
            'preprocessor' => $common,
            'meta' => $common,
            'value double' => $inline,
            'value single' => $inline,
        ));
        
        $this->addRules(array(
            'doctype' => '/<!DOCTYPE.*?>/',
            'comment' => '/<!--.*?-->/s',
            'cdata' => '/<!\[CDATA\[.*?\]\]>/',
            'tag' => new Rule('/</', '/>/'),
            'tagname' => '#(?:(?<=<)|(?<=</)|(?<=<\?)|(?<=<!))[a-z0-9:-]+#i',
            'attribute' => '/[a-z0-9:-]+/i',
            'preprocessor' => new Rule('/\?/'),
            'meta' => new Rule('/!/'),
            'value' => array(
                'double' => new Rule('/"/', '/"/'),
                'single' => new Rule("/'/", "/'/")
            ),
            'entity' => '/&.*?;/',
        ));

        $this->addMappings(array(
            'attribute' => 'keyword type',
            'cdata' => '',
            'value' => 'attribute value',
            'value double' => 'attribute value string',
            'value single' => 'attribute value string',
            'entity' => 'escaped',
            'tagname' => 'keyword'
        ));
    }
}
