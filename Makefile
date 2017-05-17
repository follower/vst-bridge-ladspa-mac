#
# Nothing fancy...just build the darn thing...that's all
#

#
# Change this to the directory where the VST aeffect.h file lives
#
VSTINC=../vstsdk2.4/pluginterfaces/vst2.x/

VERSION=1.1
LADSPAINC=.
CFLAGS=-Wall -O9 -I$(LADSPAINC) -I$(VSTINC)
#CFLAGS=-Wall -O0 -g -I$(LADSPAINC) -I$(VSTINC)
SYS=$(shell uname -s)

all:
ifeq ($(SYS),Darwin)
	g++ $(CFLAGS) -arch i386 -arch ppc -isysroot /Developer/SDKs/MacOSX10.4u.sdk -framework Carbon -bundle -o vst-bridge.so vst-bridge.cpp
else
	g++ $(CFLAGS) -shared -o vst-bridge.so vst-bridge.cpp
	strip vst-bridge.so
endif

#
# Cleanup
#
clean: 
	-rm *.so

#
# Build the source dist
#
dist: clean
	-rm -rf /tmp/vst-bridge-$(VERSION) ../vst-bridge-$(VERSION).tar.gz
	mkdir /tmp/vst-bridge-$(VERSION)
	cp -pR * /tmp/vst-bridge-$(VERSION)
	tar -c -C /tmp -f ../vst-bridge-$(VERSION).tar vst-bridge-$(VERSION)
	gzip -9 ../vst-bridge-$(VERSION).tar
	rm -rf /tmp/vst-bridge-$(VERSION)

#
# Build the Mac dist
#
mac: all
	-rm -rf vst-bridge-$(VERSION) ../vst-bridge-$(VERSION).dmg
	mkdir vst-bridge-$(VERSION)
	cp -pR README.txt LICENSE.txt vst-bridge.so vst-bridge-$(VERSION)
	hdiutil create -ov -srcdir "vst-bridge-$(VERSION)" -fs HFS+ -volname "VST Bridge $(VERSION)" TMP.dmg
	hdiutil convert TMP.dmg -format UDZO -imagekey zlib-level=9 -o "../vst-bridge-$(VERSION).dmg"
	hdiutil internet-enable -yes "../vst-bridge-$(VERSION).dmg"
	rm -rf vst-bridge-$(VERSION) TMP.dmg

#
# Build the Linux dist
#
linux: all
	-rm -rf vst-bridge-$(VERSION) ../vst-bridge-$(VERSION).tar.gz
	mkdir vst-bridge-$(VERSION)
	cp -pR README.txt LICENSE.txt lin/vst-bridge.so vst-bridge-$(VERSION)
	tar cf ../vst-bridge-$(VERSION)-linux.tar vst-bridge-$(VERSION)
	gzip -9 ../vst-bridge-$(VERSION)-linux.tar
	rm -rf vst-bridge-$(VERSION)
