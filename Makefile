CXX   = g++
#DEBUG_FLAGS = -DDEBUG
CPPFLAGS +=     -fno-rtti              \
		-fno-exceptions        \
		-shared                \
                -fPIC

DEPENDENCY_CFLAGS = `pkg-config --cflags libxul libxul-unstable`
#KDE_CFLAGS = -I/usr/include/kde
KDE_LDFLAGS = -lkdeinit4_kwalletd
XUL_LDFLAGS = `pkg-config --libs libxul libxul-unstable`
VERSION = 0.2
FILES = KDEWallet.cpp 

TARGET = libkdewallet.so
XPI_TARGET = kde-wallet_password_integration-$(VERSION).xpi
ARCH := $(shell uname -m)
# Update the ARCH variable so that the Mozilla architectures are used
ARCH := $(shell echo ${ARCH} | sed 's/i686/x86/')

build-xpi: build-library
	sed -i 's/<em:version>.*<\/em:version>/<em:version>$(VERSION)<\/em:version>/' xpi/install.rdf
	sed -i 's/<em:targetPlatform>.*<\/em:targetPlatform>/<em:targetPlatform>Linux_$(ARCH)-gcc3<\/em:targetPlatform>/' xpi/install.rdf
	mkdir -p xpi/platform/Linux_$(ARCH)-gcc3/components
	cp $(TARGET) xpi/platform/Linux_$(ARCH)-gcc3/components
	cd xpi && zip -r ../$(XPI_TARGET) *

build-library: 
	$(CXX) $(FILES) -g -Wall -o $(TARGET) $(DEPENDENCY_CFLAGS) $(XUL_LDFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(GECKO_DEFINES) $(KDE_CFLAGS) $(KDE_LDFLAGS) $(DEBUG_FLAGS)
	chmod +x $(TARGET)
#	strip $(TARGET)

build: build-library build-xpi
 
clean: 
	rm $(TARGET)
	rm xpi/platform/Linux_$(ARCH)-gcc3/components/$(TARGET)
	rm kde-wallet_password_integration-$(VERSION).xpi
