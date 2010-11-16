TEMPLATE = lib
CONFIG += plugin
DESTDIR = ../../bin/plugins_preprocessor
unix {
	QMAKE_CXXFLAGS_RELEASE -= -O2
	QMAKE_CXXFLAGS_RELEASE += -O3 \
		 -march=native \
		 -Wno-unused-function \
		 -fopenmp
	QMAKE_CXXFLAGS_DEBUG += -Wno-unused-function \
		 -fopenmp
}
LIBS += -fopenmp
HEADERS += contractionhierarchiesturn.h \
	 dynamicturngraph.h \
	 contractionturncleanup.h \
	 ../contractionhierarchies/blockcache.h \
	 ../contractionhierarchies/binaryheap.h \
	 turncontractor.h \
    interfaces/ipreprocessor.h \
    utils/coordinates.h \
    utils/config.h \
    chtsettingsdialog.h \
    compressedturngraph.h \
    compressedturngraphbuilder.h \
    utils/bithelpers.h \
    utils/qthelpers.h \
    interfaces/irouter.h
SOURCES += contractionhierarchiesturn.cpp \
    chtsettingsdialog.cpp
FORMS += chtsettingsdialog.ui
