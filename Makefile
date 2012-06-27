FIREFOX_VERSION = 14
VERSION = 1.$(FIREFOX_VERSION)

BUILD_DIR = build
XPI_TARGET = kde-wallet_password_integration-$(VERSION)-fx+tb-linux.xpi
TARBAL_TARGET = kde-wallet_password_integration-$(VERSION).tar.gz
XPI_DIR = xpi
ARCH := $(shell uname -m)
# Update the ARCH variable so that the Mozilla architectures are used
ARCH := $(shell echo ${ARCH} | sed 's/i686/x86/')
LIBNAME = libkdewallet_$(ARCH).so
SOURCE = $(BUILD_DIR)/lib/libkdewallet.so
TARGET_DIR = $(XPI_DIR)/components

build: build_lib copy archive

archive: $(XPI_TARGET)

copy: $(SOURCE)
	mkdir -p $(TARGET_DIR)
	cp $(SOURCE) $(TARGET_DIR)/$(LIBNAME)

build_lib:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ../
	cd $(BUILD_DIR) && make

$(XPI_TARGET): 
	sed -i 's/<em:version>.*<\/em:version>/<em:version>$(VERSION)<\/em:version>/' $(XPI_DIR)/install.rdf
#	sed -i 's/<em:minVersion>.*<\/em:minVersion>/<em:minVersion>$(FIREFOX_VERSION).0<\/em:minVersion>/' $(XPI_DIR)/install.rdf
	sed -i 's/<em:maxVersion>.*<\/em:maxVersion>/<em:maxVersion>$(FIREFOX_VERSION)\.\*<\/em:maxVersion>/' $(XPI_DIR)/install.rdf
	rm -f $(XPI_TARGET)
	cd $(XPI_DIR) && find . \( ! -regex '.*/\..*' \) | zip ../../$(XPI_TARGET) -@

tarbal: clean
	cd .. && tar cvfz $(TARBAL_TARGET) --transform='s,firefox-kde-wallet,kwallet@guillermo.molina,' --exclude $(BUILD_DIR) --exclude '.*'  --exclude '*.so' --exclude '*.xpi' firefox-kde-wallet

clean: 
	rm -rf $(BUILD_DIR)
#	rm -f $(TARGET_DIR)/$(LIBNAME)

