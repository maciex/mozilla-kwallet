find_package(PkgConfig REQUIRED)
find_package(KDE4 REQUIRED)
find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

pkg_check_modules(LIBXUL REQUIRED libxul)
pkg_check_modules(MOZNSPR REQUIRED mozilla-nspr)
pkg_check_modules(MOZPLUGIN REQUIRED mozilla-plugin)

include_directories($KDE4_INCLUDES} ${QT_INCLUDE_DIR} ${MOZNSPR_INCLUDE_DIRS} ${MOZPLUGIN_INCLUDE_DIRS})
link_directories(${QT_LIBRARY_DIR} ${LIBXUL_LIBRARY_DIRS})
set(CMAKE_CXX_FLAGS "")
add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS} "-DMOZ_NO_MOZALLOC -fno-rtti -fno-exceptions -fPIC -std=c++0x")

add_library(kdewallet MODULE KDEWallet.cpp)
target_link_libraries(kdewallet ${QT_CORE_LIBRARIES} ${KDE4_KDEUI_LIBS} ${LIBXUL_LIBRARIES} ${MOZNSPR_LIBRARIES} ${MOZPLUGIN_LIBRARIES})
install(TARGETS kdewallet DESTINATION components PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)
