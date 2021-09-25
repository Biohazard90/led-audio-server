set(ledserver_src
	"ledservermain.cpp"
	
	"audioprocessor.cpp"
	"audioprocessor.h"
	"timer.h"
	
	"SerialPort.cpp"
	"SerialPort.hpp"

	#"icon.ico"
	#"resource.h"
	"ledserver.manifest"
)

#list(APPEND ledserver_src "ledserver.rc")

include_directories(".")
include_directories("include")
include_directories("../ffts/include")

ADD_MSVC_PRECOMPILED_HEADER("platform.h" "${CMAKE_SOURCE_DIR}/server/platform.cpp" ledserver_src)

set(LINKER_DELAY_LOADING "")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE /SUBSYSTEM:WINDOWS ${LINKER_DELAY_LOADING} ")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /OPT:REF /OPT:ICF /NODEFAULTLIB:\"LIBCMT\"")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /SAFESEH:NO /NODEFAULTLIB:\"LIBCMT\"")

set(projoutputname "ledserver${BITS}${provider_executable_suffix}")
add_definitions(-DCMAKE_BINARY_NAME="${projoutputname}")
add_executable(${projoutputname} ${ledserver_src})
target_link_libraries(${projoutputname} optimized libcmt)

target_link_libraries(${projoutputname} ffts)

ADD_TARGET_OUTPUT(${projoutputname})
ADD_SOURCE_DIRS(ledserver_src)