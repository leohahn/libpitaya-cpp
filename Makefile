SRC_FILES = $(shell find src test -name '*.cpp')
HDR_FILES = $(shell find include test -not -path 'include/protos/*' -name '*.h')

format:
	@for f in ${SRC_FILES} ; do \
		clang-format -i $$f ; \
	done
	@for f in ${HDR_FILES} ; do \
		clang-format -i $$f ; \
	done

clean-xcode:
	@rm -rf _builds/xcode

generate-xcode:
	@conan install . -if _builds/xcode
	@cmake -H. -B_builds/xcode -GXcode -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF

build-mac:
	@mkdir -p _builds/mac
	@conan install -if _builds/mac .
	@cmake -H. -B_builds/mac -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF
	@cmake --build _builds/mac

