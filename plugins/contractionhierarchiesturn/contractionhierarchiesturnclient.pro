TEMPLATE = lib
CONFIG += plugin static
DESTDIR = ../../bin/plugins_client
unix {
	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE += -O3 \
		 -Wno-unused-function
	QMAKE_CXXFLAGS_DEBUG += -Wno-unused-function
}

HEADERS += \
    utils/coordinates.h \
    utils/config.h \
    ../contractionhierarchies/blockcache.h \
    ../contractionhierarchies/binaryheap.h \
    interfaces/irouter.h \
    contractionhierarchiesturnclient.h \
    compressedturngraph.h \
    interfaces/igpslookup.h \
    utils/bithelpers.h \
    utils/qthelpers.h

SOURCES += \
    contractionhierarchiesturnclient.cpp
