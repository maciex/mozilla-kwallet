CXX   = g++
DEBUG_FLAGS = -DDEBUG
CXX_CFLAGS +=     -fno-rtti              \
		-fno-exceptions        \
		-shared                \
                -fPIC                  \
		-std=c++0x

XUL_CFLAGS = -DMOZ_NO_MOZALLOC `pkg-config --cflags libxul` 
XUL_LDFLAGS = `pkg-config --libs libxul`

KDE_CFLAGS = `pkg-config --cflags QtCore`
KDE_LDFLAGS = -lkdeinit4_kwalletd

VERSION = 1.0.8
FILES = KDEWallet.cpp

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
	$(CXX) -c KDEWallet.cpp -g $(XUL_CFLAGS) $(CXX_CFLAGS) $(KDE_CFLAGS) $(DEBUG_FLAGS)
	ld -o $(TARGET) $(XUL_LDFLAGS) $(KDE_LDFLAGS) -shared
	chmod +x $(TARGET) 

clean: 
	rm -f $(TARGET)
	rm -f KDEWallet.o
	rm -f $(XPI_DIR)/components
	rm -fr $(XPI_DIR)/platform
	rm -f $(XPI_TARGET)

tarbal:
	cd ../.. && rm -f $(TARBAL_TARGET) && tar cvfz $(TARBAL_TARGET) --transform='s,firefox-kde-wallet,kwallet@guillermo.molina,' --exclude '.*'  --exclude '*.so' --exclude '*.xpi' firefox-kde-wallet
