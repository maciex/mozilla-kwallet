VERSION = 1.0.8

BUILD_DIR = build
XPI_TARGET = $(BUILD_DIR)/kde-wallet_password_integration-$(VERSION).xpi
TARBAL_TARGET = $(BUILD_DIR)/kde-wallet_password_integration-$(VERSION).tar.gz
XPI_DIR = xpi
ARCH := $(shell uname -m)
# Update the ARCH variable so that the Mozilla architectures are used
ARCH := $(shell echo ${ARCH} | sed 's/i686/x86/')
LIBNAME = libkdewallet_$(ARCH).so
SOURCE = $(BUILD_DIR)/lib/libkdewallet.so
TARGET_DIR = $(XPI_DIR)/components

build: $(XPI_TARGET)

$(XPI_TARGET): $(TARGET)
	sed -i 's/<em:version>.*<\/em:version>/<em:version>$(VERSION)<\/em:version>/' $(XPI_DIR)/install.rdf
	mkdir -p $(TARGET_DIR)
	cp $(SOURCE) $(TARGET_DIR)/$(LIBNAME)
	rm -f $(XPI_TARGET)
	cd $(XPI_DIR) && find . \( ! -regex '.*/\..*' \) | zip ../$(XPI_TARGET) -@

clean: 
	rm -rf $(BUILD_DIR)
#	rm -f $(TARGET_DIR)/$(LIBNAME)

tarbal:
	cd .. && tar cvfz $(TARBAL_TARGET) --transform='s,firefox-kde-wallet,kwallet@guillermo.molina,' --exclude $(BUILD_DIR) --exclude '.*'  --exclude '*.so' --exclude '*.xpi' firefox-kde-wallet
