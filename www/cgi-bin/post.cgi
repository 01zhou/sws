#!/usr/bin/perl

read(STDIN, $line, $ENV{'CONTENT_LENGTH'});
@values = split(/&/, $line);
print "Content-type:text/html\n\n";

print "<html><head><title>Parsing with POST method</title></head>";
print "<body>\n";
print "<h2>These were the values sent:</h2>\n";
foreach $i (@values) {
	($varname, $data) = split(/=/, $i);
	print "<p>$varname => $data</p>\n";
}
print "</p>\n";
print "</body></html>\n";
