#! /bin/sh
#
# (LW 18/04/2002) Stripped trailing slashes from comments, to keep make happy
# (JH 16/08/2002) Patch submitted by Stefan Asserhall to deal with diacritic characters properly
# (JH 16/08/2002) modified to sort strings alphabetically and filter through uniq.
# (HE 31/08/2002) treat cities, regions, countries separatedly

rm -f kstars_i18n.cpp
rm -f cities.tmp
rm -f regions.tmp
rm -f countries.tmp
echo "#if 0" >> kstars_i18n.cpp

# extract constellations
sed -e "s/\([0-9].*[a-z]\)//" < data/cnames.dat | sed 's/^[A-B] //' | \
   sed 's/\([A-Z].*\)/i18n("Constellation name (optional)", "\1");/' | sed 's/\ "/"/g' >> "kstars_i18n.cpp"

# extract cities
awk 'BEGIN {FS=":"}; {print "\"" $1 "\""; }' < data/Cities.dat | \
   sed 's/ *\"$/\");/g' | sed 's/^\" */i18n(\"City name (optional, probably does not need a translation)\",\"/g' | sed 's/i18n(.*,"");//' >> "cities.tmp"
sort --unique cities.tmp >> kstars_i18n.cpp

# extract regions
awk 'BEGIN {FS=":"}; {print "\"" $2 "\""; }' < data/Cities.dat | \
   sed 's/ *\"$/\");/g' | sed 's/^\" */i18n(\"Region\/state name (optional, rarely needs a translation)\",\"/g' | sed 's/i18n(.*,"");//' >> "regions.tmp";
sort --unique regions.tmp >> kstars_i18n.cpp

# extract countries
awk 'BEGIN {FS=":"}; {print "\"" $3 "\""; }' < data/Cities.dat | \
   sed 's/ *\"$/\");/g' | sed 's/^\" */i18n(\"Country name (optional, but should be translated)\",\"/g' | sed 's/i18n(.*,"");//' >> "countries.tmp"
sort --unique countries.tmp >> kstars_i18n.cpp

# extract image/info menu items
awk 'BEGIN {FS=":"}; {print "i18n(\"Image/info menu item (should be translated)\",\"" $2 "\");"; }' < data/image_url.dat | \
    sed 's/i18n(.*,"");//' >> "image_url.tmp"
sort --unique image_url.tmp >> kstars_i18n.cpp
awk 'BEGIN {FS=":"}; {print "i18n(\"Image/info menu item (should be translated)\",\"" $2 "\");"; }' < data/info_url.dat | \
    sed 's/i18n(.*,"");//' >> "info_url.tmp"
sort --unique info_url.tmp >> kstars_i18n.cpp

# star names : some might be different in other languages, or they might have to be adapted to non-Latin alphabets
cat data/hip*.dat | perl -e 'while ( $line=<STDIN> ) { $starname = substr ($line,72);    chop $starname; if ( $starname =~ /(.*)\:/ ) { $starname = $1 . " ";   }   if ( $starname =~ /(.*\w)(\s+)/) { $starname = $1;	$starnames{$starname} = 1;   } } foreach $star( sort keys %starnames) { printf "i18n(\"star name\",\"%s\");\n", $star; }' >> kstars_i18n.cpp;
# extract deep-sky object names (sorry, I don't know perl-fu ;( ...using AWK )
cat data/ngcic*.dat | gawk '{ split(substr( $0, 77 ), name, " "); \
if ( name[1]!="" ) { \
printf( "%s", name[1] ); i=2; \
while( name[i]!="" ) { printf( " %s", name[i] ); i++; } \
printf( "\n" ); } }' | sort --unique | gawk '{ \
printf( "i18n(\"object name (optional)\", \"%s\");\n", $0 ); }' >> kstars_i18n.cpp

# extract strings from file containing advanced URLs:
cat data/advinterface.dat | gawk '( match( $0, "KSLABEL" ) ) { \
name=substr($0,10) \
printf( "i18n(\"Advanced URLs: description or category\", \"%s\")\n", name ); }' >> kstars_i18n.cpp

# finish file
echo "#endif" >> kstars_i18n.cpp
# cleanup temporary files
rm -f cities.tmp
rm -f regions.tmp
rm -f countries.tmp
rm -f image_url.tmp
rm -f info_url.tmp
rm -f tips.cpp

$EXTRACTRC *.ui tools/*.ui *.rc >> rc.cpp || exit 11
(cd data && $PREPARETIPS > ../tips.cpp)
$XGETTEXT *.cpp *.h tools/*.cpp tools/*.h -o $podir/kstars.pot
rm -f tips.cpp
rm -f kstars_i18n.cpp

