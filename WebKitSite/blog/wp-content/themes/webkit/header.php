<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
    <meta http-equiv="Content-Type" content="text/html;charset=utf-8">

    <title><?php bloginfo('name'); if ( is_single() ) { ?> - Blog Archive <?php } wp_title(); ?></title>

    <meta name="generator" content="WordPress <?php bloginfo('version'); ?>">

    <link rel="stylesheet" type="text/css" href="/css/main.css">
    <link rel="stylesheet" type="text/css" href="/css/green.css" title="green">
    <link rel="stylesheet" type="text/css" href="<?php bloginfo('stylesheet_url'); ?>">

    <link rel="alternate stylesheet" type="text/css" href="/css/blue.css" title="blue">
    <link rel="alternate stylesheet" type="text/css" href="/css/yellow.css" title="yellow">
    <link rel="alternate stylesheet" type="text/css" href="/css/pink.css" title="pink">
    <link rel="alternate stylesheet" type="text/css" href="/css/purple.css" title="purple">
    <link rel="alternate stylesheet" type="text/css" href="/css/gray.css" title="gray">

<!--[if gte IE 5]>
    <link rel="stylesheet" type="text/css" href="/css/ie.css">
<![endif]-->

    <link rel="alternate" type="application/rss+xml" title="RSS 2.0" href="<?php bloginfo('rss2_url'); ?>">
    <link rel="alternate" type="text/xml" title="RSS .92" href="<?php bloginfo('rss_url'); ?>">
    <link rel="alternate" type="application/atom+xml" title="Atom 0.3" href="<?php bloginfo('atom_url'); ?>">
    <link rel="pingback" href="<?php bloginfo('pingback_url'); ?>">

    <?php wp_head(); ?>
</head>
<body>

<div id="title">
<h1><?php bloginfo('name'); ?></h1>
</div>

<div id="icon"></div>

<div id="content">
