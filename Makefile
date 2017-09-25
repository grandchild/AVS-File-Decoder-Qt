
default: debug

debug:
	mkdir -p build/debug
	qmake AvsDecoder.pro CONFIG+=debug CONFIG+=qml_debug -o build/debug/Makefile
	cd build/debug && \
		make qmake_all && \
		make && \
		make install

release:
	mkdir -p build/release
	qmake AvsDecoder.pro -o build/release/Makefile
	cd build/release && \
		make qmake_all && \
		make && \
		make install

run: release
	cd build/release && ./AvsDecoder
