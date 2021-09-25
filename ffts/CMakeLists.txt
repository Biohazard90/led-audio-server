set(ffts_src
	include/ffts.h
	
	src/ffts_attributes.h
	src/ffts.c
	src/ffts_chirp_z.c
	src/ffts_chirp_z.h
	src/ffts_internal.h
	src/ffts_nd.c
	src/ffts_nd.h
	src/ffts_real.h
	src/ffts_real.c
	src/ffts_real_nd.c
	src/ffts_real_nd.h
	src/ffts_transpose.c
	src/ffts_transpose.h
	src/ffts_trig.c
	src/ffts_trig.h
	src/ffts_static.c
	src/ffts_static.h
	src/macros.h
	src/patterns.h
	src/types.h
	
    #src/macros-sse.h
)

include_directories("include")

add_definitions(-DDYNAMIC_DISABLED)
add_definitions(-DHAVE_SSE)
add_definitions(-DHAVE_SSE2)



include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
add_definitions(-DFFTS_BUILD)

# check existence of various headers
CHECK_INCLUDE_FILE(malloc.h    HAVE_MALLOC_H)
CHECK_INCLUDE_FILE(mm_malloc.h HAVE_MM_MALLOC_H)
CHECK_INCLUDE_FILE(stdint.h    HAVE_STDINT_H)
CHECK_INCLUDE_FILE(stdlib.h    HAVE_STDLIB_H)
CHECK_INCLUDE_FILE(string.h    HAVE_STRING_H)
CHECK_INCLUDE_FILE(sys/mman.h  HAVE_SYS_MMAN_H)
CHECK_INCLUDE_FILE(unistd.h    HAVE_UNISTD_H)

if(HAVE_MALLOC_H)
  add_definitions(-DHAVE_MALLOC_H)
endif(HAVE_MALLOC_H)

if(HAVE_MM_MALLOC_H)
  add_definitions(-DHAVE_MM_MALLOC_H)
endif(HAVE_MM_MALLOC_H)

if(HAVE_STDINT_H)
  add_definitions(-DHAVE_STDINT_H)
endif(HAVE_STDINT_H)

if(HAVE_STDLIB_H)
  add_definitions(-DHAVE_STDLIB_H)
endif(HAVE_STDLIB_H)

if(HAVE_STRING_H)
  add_definitions(-DHAVE_STRING_H)
endif(HAVE_STRING_H)

if(HAVE_SYS_MMAN_H)
  add_definitions(-DHAVE_SYS_MMAN_H)
endif(HAVE_SYS_MMAN_H)

if(HAVE_UNISTD_H)
  add_definitions(-DHAVE_UNISTD_H)
endif(HAVE_UNISTD_H)

# check existence of various declarations
check_symbol_exists(aligned_alloc  stdlib.h HAVE_DECL_ALIGNED_ALLOC)
check_symbol_exists(memalign       malloc.h HAVE_DECL_MEMALIGN)
check_symbol_exists(posix_memalign stdlib.h HAVE_DECL_POSIX_MEMALIGN)
check_symbol_exists(valloc         stdlib.h HAVE_DECL_VALLOC)
check_symbol_exists(_mm_malloc     malloc.h HAVE_DECL__MM_MALLOC)

if(HAVE_DECL_ALIGNED_ALLOC)
  add_definitions(-DHAVE_DECL_ALIGNED_ALLOC)
endif(HAVE_DECL_ALIGNED_ALLOC)

if(HAVE_DECL_MEMALIGN)
  add_definitions(-DHAVE_DECL_MEMALIGN)
endif(HAVE_DECL_MEMALIGN)

if(HAVE_DECL_POSIX_MEMALIGN)
  add_definitions(-DHAVE_DECL_POSIX_MEMALIGN)
endif(HAVE_DECL_POSIX_MEMALIGN)

if(HAVE_DECL_VALLOC)
  add_definitions(-DHAVE_DECL_VALLOC)
endif(HAVE_DECL_VALLOC)

if(HAVE_DECL__MM_MALLOC)
  add_definitions(-DHAVE_DECL__MM_MALLOC)
endif(HAVE_DECL__MM_MALLOC)

# check existence of various functions
check_function_exists(aligned_alloc  HAVE_ALIGNED_ALLOC)
check_function_exists(memalign       HAVE_MEMALIGN)
check_function_exists(posix_memalign HAVE_POSIX_MEMALIGN)
check_function_exists(valloc         HAVE_VALLOC)
check_function_exists(_mm_malloc     HAVE__MM_MALLOC)

if(HAVE_ALIGNED_ALLOC)
  add_definitions(-DHAVE_ALIGNED_ALLOC)
endif(HAVE_ALIGNED_ALLOC)

if(HAVE_MEMALIGN)
  add_definitions(-DHAVE_MEMALIGN)
endif(HAVE_MEMALIGN)

if(HAVE_POSIX_MEMALIGN)
  add_definitions(-DHAVE_POSIX_MEMALIGN)
endif(HAVE_POSIX_MEMALIGN)

if(HAVE_VALLOC)
  add_definitions(-DHAVE_VALLOC)
endif(HAVE_VALLOC)

if(HAVE__MM_MALLOC)
  add_definitions(-DHAVE__MM_MALLOC)
endif(HAVE__MM_MALLOC)



add_library(ffts ${ffts_src})

ADD_SOURCE_DIRS(ffts_src)