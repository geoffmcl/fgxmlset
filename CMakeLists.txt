# fgxmlset project
# CMakeLists.txt, generated gencmake.pl, on 2014/10/28 18:12:12
cmake_minimum_required( VERSION 3.6 )

project( fgxmlset )

# The version number.
set( fgxmlset_MAJOR 3 )
set( fgxmlset_MINOR 0 )
set( fgxmlset_POINT 0 )

# have additional cmake modules...
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# Allow developer to select is Dynamic or static library built
set( LIB_TYPE STATIC )  # set default static
option( BUILD_SHARED_LIB "Build Shared Library" OFF )
# EXPERIMENTAL - Turn ON to combine library into EXE - above shared library MUST be OFF
option( BUILD_AS_ONE "Build as one. Combine library into EXE" OFF )

if(BUILD_SHARED_LIB)
   set(LIB_TYPE SHARED)
   message(STATUS "*** Building DLL library ${LIB_TYPE}")
else()
    add_definitions( -DLIBXML_STATIC )
   message(STATUS "*** Option BUILD_SHARED_LIB is OFF ${LIB_TYPE}")
endif()

if(CMAKE_COMPILER_IS_GNUCXX)
    set( WARNING_FLAGS -Wall )
endif(CMAKE_COMPILER_IS_GNUCXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang") 
   set( WARNING_FLAGS "-Wall -Wno-overloaded-virtual" )
endif() 

if(WIN32 AND MSVC)
    # turn off various warnings - none needed in this compile
    set(WARNING_FLAGS "${WARNING_FLAGS} /wd4996")
    # foreach(warning 4244 4251 4267 4275 4290 4786 4305)
    #     set(WARNING_FLAGS "${WARNING_FLAGS} /wd${warning}")
    # endforeach(warning)
    # other flags -D_SCL_SECURE_NO_WARNINGS -D__CRT_NONSTDC_NO_WARNINGS
    set( MSVC_FLAGS "-DNOMINMAX -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS" )
    #set( NOMINMAX 1 )
    set( CMAKE_DEBUG_POSTFIX "d" )
else()
    #  unix things
endif()

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS} ${MSVC_FLAGS} -D_REENTRANT" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNING_FLAGS} ${MSVC_FLAGS} -D_REENTRANT" )
set( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MSVC_LD_FLAGS}" )

#  LIBXML2_FOUND - System has LibXml2
#  LIBXML2_INCLUDE_DIR - The LibXml2 include directory
#  LIBXML2_LIBRARIES - The libraries needed to use LibXml2
#  LIBXML2_DEFINITIONS - Compiler switches required for using LibXml2
#  LIBXML2_XMLLINT_EXECUTABLE - The XML checking tool xmllint coming with LibXml2
#  LIBXML2_VERSION_STRING - the version of LibXml2 found (since CMake 2.8.8)
find_package( LibXml2 )
if (LIBXML2_FOUND)
    message(STATUS "*** LibXML2 found ver ${LIBXML2_VERSION_STRING} lib ${LIBXML2_LIBRARIES}")
    message(STATUS "*** include_directories( ${LIBXML2_INCLUDE_DIR} )")
    list( APPEND add_LIBS ${LIBXML2_LIBRARIES} )
    include_directories( ${LIBXML2_INCLUDE_DIR} )
    if (LIBXML2_DEFINITIONS) 
        add_definitions( ${LIBXML2_DEFINITIONS} )
        message(STATUS "*** add_definitions( ${LIBXML2_DEFINITIONS} )")
    endif ()
    string(FIND "${LIBXML2_LIBRARIES}" "libxml2s" GOT_STATIC)
    if(GOT_STATIC)
        message(STATUS "*** Found static, setting -DLIBXML_STATIC")
        add_definitions( -DLIBXML_STATIC )
    endif()
else ()
    message(STATUS "*** NOT FOUND LibXML2 ver '${LIBXML2_VERSION_STRING}' lib '${LIBXML2_LIBRARIES}' inc '${LIBXML2_INCLUDE_DIR}'")
    message(FATAL_ERROR "*** UGH: LibXML2 NOT FOUND!")
endif ()

#  ZLIB_FOUND          - True if zlib found.
#  ZLIB_INCLUDE_DIRS   - where to find zlib.h, etc.
#  ZLIB_LIBRARIES      - List of libraries when using zlib.
#  ZLIB_STATICLIB      - List of libraries when using zlibstatic.
find_package(ZLIB)
if (ZLIB_FOUND)
    message(STATUS "*** ZLIB found. inc ${ZLIB_INCLUDE_DIRS} lib ${ZLIB_LIBRARIES}")
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    if (ZLIB_STATICLIB)
        list( APPEND add_LIBS ${ZLIB_STATICLIB} )
        message(STATUS "*** ZLIB found STATIC lib ${ZLIB_STATICLIB}")        
    else ()
        list( APPEND add_LIBS ${ZLIB_LIBRARIES} )
        add_definitions( -DZLIB_DLL )
        message(STATUS "*** ZLIB found only SHARED lib - set -DZLIB_DLL")
    endif ()
else ()
    message(STATUS "*** ZLIB not found!")
endif ()

# SIMGEAR_FOUND, if false, do not try to link to SimGear
# SIMGEAR_CORE_LIBRARIES, a list of the core static libraries
# SIMGEAR_LIBRARIES, a list of all the static libraries (core + scene)
# SIMGEAR_INCLUDE_DIRS, where to find the headers
find_package(SimGear)
if (SIMGEAR_FOUND)
    message(STATUS "*** SimGear FOUND. inc ${SIMGEAR_INCLUDE_DIRS} set -DUSE_SIMGEAR_LIB=1")
    message(STATUS "*** Lib: core ${SIMGEAR_CORE_LIBRARIES}")
    include_directories( ${SIMGEAR_INCLUDE_DIRS} )
    list( APPEND add_LIBS ${SIMGEAR_CORE_LIBRARIES} )
    add_definitions( -DUSE_SIMGEAR_LIB=1 )
else ()
    message(STATUS "*** SimGear NOT FOUND!")
endif ()

# configuration file, if needed
configure_file( ${CMAKE_SOURCE_DIR}/config.h.cmake ${CMAKE_BINARY_DIR}/config.h )
add_definitions( -DHAVE_CONFIG_H )
include_directories( ${CMAKE_BINARY_DIR} )

# to distinguish between debug and release lib
if (MSVC)
    set( CMAKE_DEBUG_POSTFIX "d" )
endif ()

# Utility Library
set(name util_lib)
set(dir src/utils)
include_directories( ${dir} )
set(${name}_SRCS
    ${dir}/sprtf.cxx
    ${dir}/gen_utils.cxx
    )
set(${name}_HDRS
    ${dir}/sprtf.hxx
    ${dir}/gen_utils.hxx
    )
add_library( ${name} ${${name}_SRCS} ${${name}_HDRS} )
list( APPEND add_LIBS ${name} )

# EXECUTABLES from [F:\FG\18\fgxmlset\src]
set(name reader1)
set(dir src)
add_executable( ${name} ${dir}/${name}.c )
target_link_libraries( ${name} ${add_LIBS} )
if (MSVC)
    set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
endif ()
# deal with install, if any...
#install( TARGETS ${name} DESTINATION bin )


set(name fgxmlset)
set(dir src)
set(${name}_SRCS
    ${dir}/fgxmlset.cxx
    )
set(${name}_HDRS
    ${dir}/fgxmlset.hxx
    )
add_executable( ${name} ${${name}_SRCS} ${${name}_HDRS} )
target_link_libraries( ${name} ${add_LIBS} )
if (MSVC)
    set_target_properties( ${name} PROPERTIES DEBUG_POSTFIX d )
endif ()
# deal with install, if any...
install( TARGETS ${name} DESTINATION bin )

# eof
