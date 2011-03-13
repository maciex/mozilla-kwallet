CXX   = g++
DEBUG_FLAGS = -DDEBUG
CPPFLAGS +=     -fno-rtti              \
		-fno-exceptions        \
		-shared                \
                -fPIC

DEPENDENCY_CFLAGS = `pkg-config --cflags libxul` -DMOZ_NO_MOZALLOC
KDE_CFLAGS = `pkg-config --cflags QtCore`
KDE_LDFLAGS = -L/usr/lib/kde4/libkdeinit -lkdeinit4_kwalletd
XUL_LDFLAGS = `pkg-config --libs libxul`
VERSION = 0.7
FILES = KDEWallet.cpp Xul_2.cpp

BUILD_DIR = ../build
XPI_TARGET = $(BUILD_DIR)/kde-wallet_password_integration-$(VERSION).xpi
TARBAL_TARGET = kde-wallet_password_integration-$(VERSION).tar.gz
XPI_DIR = ../xpi
ARCH := $(shell uname -m)
# Update the ARCH variable so that the Mozilla architectures are used
ARCH := $(shell echo ${ARCH} | sed 's/i686/x86/')
TARGET = libkdewallet_$(ARCH).so

build: build-xpi

build-xpi: build-library
	sed -i 's/<em:version>.*<\/em:version>/<em:version>$(VERSION)<\/em:version>/' $(XPI_DIR)/install.rdf
	mkdir -p $(XPI_DIR)/components
	cp $(TARGET) $(XPI_DIR)/components
	rm -f $(XPI_TARGET)
	cd $(XPI_DIR) && find . \( ! -regex '.*/\..*' \) | zip $(XPI_TARGET) -@

build-library: 
	$(CXX) $(FILES) -g -Wall -o $(TARGET) $(DEPENDENCY_CFLAGS) $(XUL_LDFLAGS) $(CPPFLAGS) $(CXXFLAGS) $(GECKO_DEFINES) $(KDE_CFLAGS) $(KDE_LDFLAGS) $(DEBUG_FLAGS)
	chmod +x $(TARGET)
#	strip $(TARGET)
 
clean: 
	rm -f $(TARGET)
	rm -f $(XPI_DIR)/components
	rm -fr $(XPI_DIR)/platform
	rm -f $(XPI_TARGET)

tarbal:
	cd ../.. && rm -f $(TARBAL_TARGET) && tar cvfz $(TARBAL_TARGET) --transform='s,firefox-kde-wallet,kwallet@guillermo.molina,' --exclude '.*'  --exclude '*.so' firefox-kde-wallet
