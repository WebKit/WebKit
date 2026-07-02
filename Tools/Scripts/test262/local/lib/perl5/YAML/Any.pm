use strict; use warnings;
package YAML::Any;
our $VERSION = '1.24';

use Exporter ();

@YAML::Any::ISA       = 'Exporter';
@YAML::Any::EXPORT    = qw(Dump Load);
@YAML::Any::EXPORT_OK = qw(DumpFile LoadFile);

my @dump_options = qw(
    UseCode
    DumpCode
    SpecVersion
    Indent
    UseHeader
    UseVersion
    SortKeys
    AnchorPrefix
    UseBlock
    UseFold
    CompressSeries
    InlineSeries
    UseAliases
    Purity
    Stringify
);

my @load_options = qw(
    UseCode
    LoadCode
    Preserve
);

my @implementations = qw(
    YAML::XS
    YAML::Syck
    YAML::Old
    YAML
    YAML::Tiny
);

sub import {
    __PACKAGE__->implementation;
    goto &Exporter::import;
}

sub Dump {
    no strict 'refs';
    no warnings 'once';
    my $implementation = __PACKAGE__->implementation;
    for my $option (@dump_options) {
        my $var = "$implementation\::$option";
        my $value = $$var;
        local $$var;
        $$var = defined $value ? $value : ${"YAML::$option"};
    }
    return &{"$implementation\::Dump"}(@_);
}

sub DumpFile {
    no strict 'refs';
    no warnings 'once';
    my $implementation = __PACKAGE__->implementation;
    for my $option (@dump_options) {
        my $var = "$implementation\::$option";
        my $value = $$var;
        local $$var;
        $$var = defined $value ? $value : ${"YAML::$option"};
    }
    return &{"$implementation\::DumpFile"}(@_);
}

sub Load {
    no strict 'refs';
    no warnings 'once';
    my $implementation = __PACKAGE__->implementation;
    for my $option (@load_options) {
        my $var = "$implementation\::$option";
        my $value = $$var;
        local $$var;
        $$var = defined $value ? $value : ${"YAML::$option"};
    }
    return &{"$implementation\::Load"}(@_);
}

sub LoadFile {
    no strict 'refs';
    no warnings 'once';
    my $implementation = __PACKAGE__->implementation;
    for my $option (@load_options) {
        my $var = "$implementation\::$option";
        my $value = $$var;
        local $$var;
        $$var = defined $value ? $value : ${"YAML::$option"};
    }
    return &{"$implementation\::LoadFile"}(@_);
}

sub order {
    return @YAML::Any::_TEST_ORDER
        if @YAML::Any::_TEST_ORDER;
    return @implementations;
}

sub implementation {
    my @order = __PACKAGE__->order;
    for my $module (@order) {
        my $path = $module;
        $path =~ s/::/\//g;
        $path .= '.pm';
        return $module if exists $INC{$path};
        eval "require $module; 1" and return $module;
    }
    croak("YAML::Any couldn't find any of these YAML implementations: @order");
}

sub croak {
    require Carp;
    Carp::croak(@_);
}

1;
