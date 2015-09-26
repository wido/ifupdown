#!/usr/bin/perl -w

use strict;

my $DEB_HOST_ARCH_OS = `dpkg-architecture -qDEB_HOST_ARCH_OS`;

$DEB_HOST_ARCH_OS =~ s/\n//;

# declarations
my $address_family = "";
my %methods = ();
my %ourmethods = ();
my $line = "";
my $arch = "";
my $match = "";

# subroutines
sub nextline {
        $line = <>;
        while($line and ($line =~ /^#/ or $line =~ /^\s*$/)) {
                $line = <>;
        }
        if (!$line) { return 0; }
        chomp $line;
        while ($line =~ m/^(.*)\\$/s) {
                my $addon = <>;
                chomp $addon;
                $line = $1 . "\n". $addon;
        }
        return 1;
}
sub our_arch {
    return ($arch eq $DEB_HOST_ARCH_OS) || ($arch eq "any")
}
sub match {
        my $line = $_[0];
        my $cmd = "$_[1]" ? "$_[1]\\b\\s*" : "";;
        my $indentexp = (@_ == 3) ? "$_[2]\\s+" : "";

        if ($line =~ /^${indentexp}${cmd}(([^\s](.*[^\s])?)?)\s*$/s) {
                $match = $1;
                return 1;
        } else {
                return 0;
        } 
}
sub get_address_family {
        $address_family = $_[0] if ($address_family eq "");
        nextline;
}
sub get_architecture {
        %ourmethods = %methods if (our_arch());
        $arch = $_[0];
        if (!our_arch) {
                %methods = ();
        } else {
                print "#include \"archcommon.h\"\n";
                print "#include \"arch${DEB_HOST_ARCH_OS}.h\"\n\n\n";
        }
        nextline;
}
sub get_method {
        my $method = $_[0];
        my $indent = ($line =~ /(\s*)[^\s]/) ? $1 : "";
        my @options = ();
        my @variables = ();

        die "Duplicate method $method\n" if ($methods{$method}++);

        nextline;
        if (match($line, "description", $indent)) {
                skip_section();
        }
        if (match($line, "options", $indent)) {
                @options = get_options();
        }
        print "static option_default _${method}_default[] = {\n";
        if (@options) {
                foreach my $o (@options) {
                        if ($o =~ m/^\s*(\S*)\s*(.*)\s+--\s+(\S[^[]*)(\s+\[([^]]*)\]\s*)?$/) {
                                my $opt = $1;
                                my $optargs = $2;
                                my $dsc = $3;
                                push @variables, $opt;
                                if ($4) {
                                        print "\t{ \"$opt\", \"$5\" },\n";
                                }
                        }
                }
        }
        print "\t{ NULL, NULL }\n";
        print "};\n";
        print "static conversion _${method}_conv[] = {\n";
        if (match($line, "conversion", $indent)) {
                while (nextline && match($line, "", "$indent  ")) {
                        my $foo = $line;
                        $foo =~ s/^\s+//;
                        $foo =~ m/^\s*(\S+)\s+(\([^)]+\)|\S+)\s*(\S+)?\s*$/;
                        my $option = $1;
                        my $fn = $2;
                        my $newoption = $3;
                        if ($fn =~ m/^\((.*)\)$/) {
                                my @params = split(/ /, $1);
                                $fn = shift(@params);
                                foreach (@params) {
                                        if ($_ =~ m/^"(.*)"$/) {
                                            $_ = $1;
                                        }
                                }
                                $fn .= (", ".scalar(@params).", (char * []){\"".join("\", \"", @params)."\"}");
                        } else {
                                $fn .= ", 0, NULL";
                        }
                        if ($newoption) {
                                $newoption =~ s/^=//;
                                die "Duplicate option use: $newoption (from $method/$option)" if (grep $_ eq $newoption, @variables);
                                push @variables, $newoption;
                                print "\t{ \"$option\", \"$newoption\", $fn },\n";
                        } else {
                                print "\t{ \"$option\", NULL, $fn },\n";
                        }
                }
        }
        print "\t\{ NULL, NULL, NULL, 0, NULL }\n";
        print "};\n";
        if (match($line, "up", $indent)) {
                get_commands(${method}, "up");
        } else {
                print "static int _${method}_up(interface_defn ifd) { return 0; }\n"
        }
        if (match($line, "down", $indent)) {
                get_commands(${method}, "down");
        } else {
                print "static int _${method}_down(interface_defn ifd) { return 0; }\n"
        }
}
sub skip_section {
        my $struct = $_[0];
        my $indent = ($line =~ /(\s*)[^\s]/) ? $1 : "";

        1 while (nextline && match($line, "", $indent));
}

sub quote_chars {
    my $string = $_[0];
    $string =~ s/\\/\\\\/g;
    $string =~ s/"/\\"/g;
    $string =~ s/\n/\\\n/g;
    return $string;
}
sub get_commands {
        my $method = $_[0];
        my $mode = $_[1];
        my $function = "_${method}_${mode}";
        my $indent = ($line =~ /(\s*)[^\s]/) ? $1 : "";

        print "static int ${function}(interface_defn *ifd, execfn *exec) {\n";

        while (nextline && match($line, "", $indent)) {
                if ( $match =~ /^(.*[^\s])\s+if\s*\((.*)\)\s*$/s ) {
                        print "if ( $2 ) {\n";
                        print "  if (!execute(\"".quote_chars($1)."\", ifd, exec) && !ignore_failures) return 0;\n";
                        print "}\n";
                } elsif ( $match =~ /^(.*[^\s])\s+elsif\s*\((.*)\)\s*$/s ) {
                        print "else if ( $2 ) {\n";
                        print "  if (!execute(\"".quote_chars($1)."\", ifd, exec) && !ignore_failures) return 0;\n";
                        print "}\n";
                } elsif ( $match =~ /^(.*[^\s])\s*$/s ) {
                        print "{\n";
                        print "  if (!execute(\"".quote_chars($1)."\", ifd, exec) && !ignore_failures) return 0;\n";
                        print "}\n";
                }
        }

        print "return 1;\n";
        print "}\n";
}
sub get_options {
        my @opts = ();
        my $indent = ($line =~ /(\s*)\S/) ? $1 : "";
        while(nextline && match($line, "", $indent)) {
                push @opts, $match;
        }
        return @opts;
}

# main code
print "#include <stddef.h>\n";
print "#include \"header.h\"\n\n\n";
nextline;
while($line) {
        if (match($line, "address_family")) {
                get_address_family $match;
                next;
        }
        if (match($line, "architecture")) {
                get_architecture $match;
                next;
        }
        if (match($line, "method")) {
                if (our_arch()) {
                        get_method $match;
                } else {
                        skip_section;
                }
                next;
        }

        # ...otherwise
        die("Unknown command \"$line\"");
}
print "static method methods[] = {\n";
%ourmethods = %methods if (our_arch());
my $method;
foreach $method (sort keys %ourmethods) {
        print <<EOF;
        {
                "$method",
                _${method}_up, _${method}_down,
                _${method}_conv, _${method}_default
        },
EOF
}
print "};\n\n";

print <<EOF;
address_family addr_${address_family} = {
        "$address_family",
        sizeof(methods)/sizeof(struct method),
        methods
};
EOF
