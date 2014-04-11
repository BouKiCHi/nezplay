#
# Makefile
#

ZIPSRC = nezplay-`date "+%Y%m%d"`src.zip
ZIPBIN = nezplay-`date "+%Y%m%d"`bin.zip


all : release

zipsrc : 
	find . -name ".DS*" -delete
	rm -f ../$(ZIPSRC)
	zip -r ../$(ZIPSRC) .

zipbin : 
	find . -name ".DS*" -delete
	rm -f ../$(ZIPBIN)
	cd sdlplay ; zip -r ../../$(ZIPBIN) nezplays nezplays.exe readme.txt

release :
	cd sdlplay ; make release

clean :
	cd sdlplay ; make clean


