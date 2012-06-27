cmake_minimum_required(VERSION 2.6)
project(firefox-kde-wallet)

set(VERSION 14)
set(BUILD 2)
set(XPI_NAME "firefox-kde-wallet-${VERSION}-${BUILD}.xpi")
set(MIN_VERSION "13.0")
set(MAX_VERSION "14.*")
set(UPDATE_URL "http://www.guillermomolina.com.ar/index.php/en/files/category/1-firefox-kwallet-extension")

add_subdirectory(src)
