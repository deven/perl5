#!./perl
print "1..3\n";

$_=join "", grep $_=chr($_), 32..127;

#95 characters - 52 letters - 10 digits = 33 backslashes
#95 characters + 33 backslashes = 128 characters
$_=quotemeta $_;
if ( length == 128 ){print "ok 1\n"} else {print "not ok 1\n"}
if (tr/\\//cd == 94){print "ok 2\n"} else {print "not ok 2\n"}

#perl5a11 bus errors on this:
if (length quotemeta "" == 0){print "ok 3\n"} else {print "not ok 3\n"}
