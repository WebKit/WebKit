package YAML::Mo;
# use Mo qw[builder default import];
#   The following line of code was produced from the previous line by
#   Mo::Inline version 0.4
no warnings;my$M=__PACKAGE__.'::';*{$M.Object::new}=sub{my$c=shift;my$s=bless{@_},$c;my%n=%{$c.'::'.':E'};map{$s->{$_}=$n{$_}->()if!exists$s->{$_}}keys%n;$s};*{$M.import}=sub{import warnings;$^H|=1538;my($P,%e,%o)=caller.'::';shift;eval"no Mo::$_",&{$M.$_.::e}($P,\%e,\%o,\@_)for@_;return if$e{M};%e=(extends,sub{eval"no $_[0]()";@{$P.ISA}=$_[0]},has,sub{my$n=shift;my$m=sub{$#_?$_[0]{$n}=$_[1]:$_[0]{$n}};@_=(default,@_)if!($#_%2);$m=$o{$_}->($m,$n,@_)for sort keys%o;*{$P.$n}=$m},%e,);*{$P.$_}=$e{$_}for keys%e;@{$P.ISA}=$M.Object};*{$M.'builder::e'}=sub{my($P,$e,$o)=@_;$o->{builder}=sub{my($m,$n,%a)=@_;my$b=$a{builder}or return$m;my$i=exists$a{lazy}?$a{lazy}:!${$P.':N'};$i or ${$P.':E'}{$n}=\&{$P.$b}and return$m;sub{$#_?$m->(@_):!exists$_[0]{$n}?$_[0]{$n}=$_[0]->$b:$m->(@_)}}};*{$M.'default::e'}=sub{my($P,$e,$o)=@_;$o->{default}=sub{my($m,$n,%a)=@_;exists$a{default}or return$m;my($d,$r)=$a{default};my$g='HASH'eq($r=ref$d)?sub{+{%$d}}:'ARRAY'eq$r?sub{[@$d]}:'CODE'eq$r?$d:sub{$d};my$i=exists$a{lazy}?$a{lazy}:!${$P.':N'};$i or ${$P.':E'}{$n}=$g and return$m;sub{$#_?$m->(@_):!exists$_[0]{$n}?$_[0]{$n}=$g->(@_):$m->(@_)}}};my$i=\&import;*{$M.import}=sub{(@_==2 and not$_[1])?pop@_:@_==1?push@_,grep!/import/,@f:();goto&$i};@f=qw[builder default import];use strict;use warnings;

our $DumperModule = 'Data::Dumper';

my ($_new_error, $_info, $_scalar_info);

no strict 'refs';
*{$M.'Object::die'} = sub {
    my $self = shift;
    my $error = $self->$_new_error(@_);
    $error->type('Error');
    Carp::croak($error->format_message);
};

*{$M.'Object::warn'} = sub {
    my $self = shift;
    return unless $^W;
    my $error = $self->$_new_error(@_);
    $error->type('Warning');
    Carp::cluck($error->format_message);
};

# This code needs to be refactored to be simpler and more precise, and no,
# Scalar::Util doesn't DWIM.
#
# Can't handle:
# * blessed regexp
*{$M.'Object::node_info'} = sub {
    my $self = shift;
    my $stringify = $_[1] || 0;
    my ($class, $type, $id) =
        ref($_[0])
        ? $stringify
          ? &$_info("$_[0]")
          : do {
              require overload;
              my @info = &$_info(overload::StrVal($_[0]));
              if (ref($_[0]) eq 'Regexp') {
                  @info[0, 1] = (undef, 'REGEXP');
              }
              @info;
          }
        : &$_scalar_info($_[0]);
    ($class, $type, $id) = &$_scalar_info("$_[0]")
        unless $id;
    return wantarray ? ($class, $type, $id) : $id;
};

#-------------------------------------------------------------------------------
$_info = sub {
    return (($_[0]) =~ qr{^(?:(.*)\=)?([^=]*)\(([^\(]*)\)$}o);
};

$_scalar_info = sub {
    my $id = 'undef';
    if (defined $_[0]) {
        \$_[0] =~ /\((\w+)\)$/o or CORE::die();
        $id = "$1-S";
    }
    return (undef, undef, $id);
};

$_new_error = sub {
    require Carp;
    my $self = shift;
    require YAML::Error;

    my $code = shift || 'unknown error';
    my $error = YAML::Error->new(code => $code);
    $error->line($self->line) if $self->can('line');
    $error->document($self->document) if $self->can('document');
    $error->arguments([@_]);
    return $error;
};

1;
