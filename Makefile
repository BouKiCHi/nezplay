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
	cd sdlplay ; zip -r ../../$(ZIPBIN) nezplay_s nezplay_s.exe readme.txt

release :
	cd sdlplay ; make release
	cd sdlplay ; make -f Makefile.w32 release

clean :
	cd sdlplay ; make clean
	cd sdlplay ; make -f Makefile.w32 clean


