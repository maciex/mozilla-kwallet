CXX   = g++
DEBUG_FLAGS = -DDEBUG
CPPFLAGS +=     -fno-rtti              \
		-fno-exceptions        \
		-shared                \
                -fPIC

DEPENDENCY_CFLAGS = `pkg-config --cflags libxul libxul-unstable`
#KDE_CFLAGS = -I/usr/include/kde
KDE_LDFLAGS = -lkdeinit4_kwalletd
XUL_LDFLAGS = `pkg-config --libs libxul libxul-unstable`
VERSION = 0.6
FILES = KDEWallet.cpp 

BUILD_DIR = ../build
TARGET = libkdewallet.so
XPI_TARGET = $(BUILD_DIR)/kde-wallet_password_integration-$(VERSION).xpi
TARBAL_TARGET = build/kde-wallet_password_integration-$(VERSION).tar.gz
XPI_DIR = ../xpi
ARCH := $(shell uname -m)
# Update the ARCH variable so that the Mozilla architectures are used
ARCH := $(shell echo ${ARCH} | sed 's/i686/x86/')

build-xpi: build-library
	sed -i 's/<em:version>.*<\/em:version>/<em:version>$(VERSION)<\/em:version>/' $(XPI_DIR)/install.rdf
#	sed -i 's/<em:targetPlatform>.*<\/em:targetPlatform>/<em:targetPlatform>Linux_$(ARCH)-gcc3<\/em:targetPlatform>/' $(XPI_DIR)/install.rdf
	mkdir -p $(XPI_DIR)/platform/Linux_$(ARCH)-gcc3/components
	cp $(TARGET) $(XPI_DIR)/platform/Linux_$(ARCH)-gcc3/components
	rm -f $(XPI_TARGET)
	cd $(XPI_DIR) && find . \( ! -regex '.*/\..*' \) | zip $(XPI_TARGET) -@

build-library: 
	$(CXX) $(FILES) -g -Wall -o $(TARGET) $(DEPENDENCY_CFLAGS) $(XUL_LDFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(GECKO_DEFINES) $(KDE_CFLAGS) $(KDE_LDFLAGS) $(DEBUG_FLAGS)
	chmod +x $(TARGET)
#	strip $(TARGET)

build: build-library build-xpi
 
clean: 
	rm -f $(TARGET)
	rm -f $(XPI_DIR)/platform/Linux_$(ARCH)-gcc3/components/$(TARGET)
	rm -f $(XPI_TARGET)
	rm -f $(TARBAL_TARGET)

tarbal:
	rm -f $(TARBAL_TARGET)
	cd .. && tar cvf $(TARBAL_TARGET) --exclude '.*'  --exclude '*.so' --exclude 'build' *
