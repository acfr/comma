# to use toolchain, run ccmake/cmake first time as:
#  ccmake -DUSE_ARM_TOOLCHAIN=ON ~/src/comma/
# for now, if you want to build applications, you will need to point pthread
# and boost libraries to the libraries built for arm
if( USE_ARM_TOOLCHAIN )
    set( CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles/arm.toolchain.cmake )
    # for info: look at opencv-2.4.11/platforms/linux/arm-gnueabi.toolchain.cmake
    include( ${CMAKE_TOOLCHAIN_FILE} REQUIRED )
endif( USE_ARM_TOOLCHAIN )

PROJECT( "comma" )

cmake_minimum_required(VERSION 2.6)

# option( USE_ARM_TOOLCHAIN "Cross compile using arm toolchain" OFF )
# if( USE_ARM_TOOLCHAIN )
#     set(CMAKE_TOOLCHAIN_FILE ${SOURCE_CODE_BASE_DIR}/CMakeFiles/arm.toolchain.cmake)
# endif( USE_ARM_TOOLCHAIN )
# 
# option(my_test_option "whatever" OFF)
# if(my_test_option)
#     set(my_other_option "is set")
# endif(my_test_option)
# set(my_first_option "is also set")

ENABLE_TESTING()

IF (UNIX AND NOT APPLE)
  # May also fail on BSD as it assumes GNU date rather than BSD date command
	# Check if Git is installed
	find_package(Git)
	IF(GIT_FOUND)
		# Get the date of the last commit (for use by CPack)
		EXECUTE_PROCESS(
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMAND git log -n1 --format=%ci
		COMMAND xargs -0 date +%Y%m%dT%H%M -d
		OUTPUT_VARIABLE COMMITDATE
		OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		# And also the hash.
		EXECUTE_PROCESS(
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMAND git rev-parse HEAD
		OUTPUT_VARIABLE COMMITHASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	ELSE()
		SET ( COMMITDATE 1 )
		SET ( COMMITHASH "abc" )
	ENDIF()
ELSE() # Probably windows; gets hardcoded patch version
	SET ( COMMITDATE 1 ) 
	SET ( COMMITHASH "abc" )
ENDIF()

# ---------------------------------------------------------------------------
# set compiler and linker flags and variables

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "options are: None Debug Release RelWithDebInfo MinSizeRel"
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

IF(WIN32)
  SET(CMAKE_CXX_WARNING_LEVEL 4)
  IF(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    STRING(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  ELSE(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  ENDIF(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
  ADD_DEFINITIONS(/wd4512)
  ADD_DEFINITIONS(/wd4127)
  ADD_DEFINITIONS(/wd4996)
  ADD_DEFINITIONS(/wd4800)
  ADD_DEFINITIONS(/wd4244)
  SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D \"NOMINMAX\"")
  ADD_DEFINITIONS(-D_USE_MATH_DEFINES)
  ADD_DEFINITIONS(-DWIN32_LEAN_AND_MEAN) #used to fix winsock redefinition error
ENDIF(WIN32)

IF( CMAKE_SIZEOF_VOID_P EQUAL 8 )
  SET( IS_64_BIT TRUE )
ELSE( CMAKE_SIZEOF_VOID_P EQUAL 8 )
  SET( IS_64_BIT FALSE )
ENDIF( CMAKE_SIZEOF_VOID_P EQUAL 8 )

IF(CMAKE_BUILD_TOOL MATCHES "make")
  IF(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  ENDIF(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")
  IF(NOT CMAKE_C_FLAGS MATCHES "-Wall")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
  ENDIF(NOT CMAKE_C_FLAGS MATCHES "-Wall")

  IF( IS_64_BIT )
    IF(NOT CMAKE_CXX_FLAGS MATCHES "-fPIC")
      SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    ENDIF(NOT CMAKE_CXX_FLAGS MATCHES "-fPIC")
    IF(NOT CMAKE_C_FLAGS MATCHES "-fPIC")
      SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    ENDIF(NOT CMAKE_C_FLAGS MATCHES "-fPIC")
  ENDIF( IS_64_BIT )

ENDIF(CMAKE_BUILD_TOOL MATCHES "make")

if( UNIX )
INCLUDE( CMakeFiles/check.c++.standard.cmake REQUIRED )
endif()

SET( SOURCE_CODE_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
SET( LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib )

SET( EXECUTABLE_OUTPUT_PATH
     ${PROJECT_BINARY_DIR}/bin CACHE PATH
     "Single output directory for building all executables." )

SET( comma_LIBRARY_PROPERTIES ${comma_LIBRARY_PROPERTIES} DEBUG_POSTFIX "-d" )

IF( NOT comma_INSTALL_BIN_DIR )
  SET( comma_INSTALL_BIN_DIR bin )
ENDIF( NOT comma_INSTALL_BIN_DIR )

IF( NOT comma_INSTALL_INCLUDE_DIR )
    SET( comma_INSTALL_INCLUDE_DIR include/${PROJECT_NAME} )
ENDIF( NOT comma_INSTALL_INCLUDE_DIR )

IF( NOT comma_INSTALL_LIB_DIR)
  SET( comma_INSTALL_LIB_DIR lib )
ENDIF( NOT comma_INSTALL_LIB_DIR )

IF( NOT comma_INSTALL_PACKAGE_DIR )
  SET( comma_INSTALL_PACKAGE_DIR CMakeFiles )
ENDIF( NOT comma_INSTALL_PACKAGE_DIR )

#INCLUDE_DIRECTORIES( ${comma_INSTALL_INCLUDE_DIR}/../ )

IF( APPLE )
    SET( CMAKE_INSTALL_NAME_DIR ${CMAKE_INSTALL_PREFIX}/${comma_INSTALL_LIB_DIR} )
ENDIF( APPLE )

OPTION( BUILD_TESTS "build unit tests" OFF )
SET( comma_BUILD_TESTS ${BUILD_TESTS} )

OPTION( BUILD_PYTHON_PACKAGES "install comma python packages" ON )

INCLUDE( CMakeDependentOption )
CMAKE_DEPENDENT_OPTION( ADD_PYTHON_PACKAGES_TO_RPM "add comma python packages to rpm" OFF "BUILD_PYTHON_PACKAGES" ON )
CMAKE_DEPENDENT_OPTION( BUILD_CPP_PYTHON_BINDINGS "build C++/Python bindings" OFF "BUILD_PYTHON_PACKAGES" ON )

OPTION( BUILD_APPLICATIONS "build applications" ON )
SET( comma_BUILD_APPLICATIONS ${BUILD_APPLICATIONS} )

OPTION( comma_BUILD_ZEROMQ "build io with zeromq support" OFF )

OPTION(BUILD_SHARED_LIBS "build with shared libraries" OFF)

OPTION( comma_BUILD_XML "build xml" OFF )

SET(comma_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

IF( NOT BUILD_SHARED_LIBS )
    SET( Boost_USE_STATIC_LIBS ON )
ENDIF( NOT BUILD_SHARED_LIBS )

IF( BUILD_PYTHON_PACKAGES AND BUILD_CPP_PYTHON_BINDINGS )
    IF( NOT BUILD_SHARED_LIBS )
        MESSAGE( WARNING "Linking C++/Python bindings with static libraries is requested. If linking fails, recompile required libraries with -fPIC or use shared libraries. Note that this feature has only been tested with shared libraries." )
    ENDIF( NOT BUILD_SHARED_LIBS )
    SET( Python_ADDITIONAL_VERSIONS 2.7 )
    FIND_PACKAGE( PythonLibs REQUIRED )
    INCLUDE_DIRECTORIES( ${PYTHON_INCLUDE_DIRS} )
    FIND_PACKAGE( Boost COMPONENTS thread filesystem system date_time program_options python )
ELSE( BUILD_PYTHON_PACKAGES AND BUILD_CPP_PYTHON_BINDINGS )
    FIND_PACKAGE( Boost COMPONENTS thread filesystem system date_time program_options )
ENDIF( BUILD_PYTHON_PACKAGES AND BUILD_CPP_PYTHON_BINDINGS )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIRS} )
LINK_DIRECTORIES( ${Boost_LIBRARY_DIRS} )

IF( comma_BUILD_TESTS )

    # todo: maybe else clause is actually the right way to do things
    IF( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "Windows" )
        FIND_PATH( GTest_SOURCE_DIR NAMES CMakeLists.txt HINTS /usr/src/gtest DOC "source code of gtest" )
        ADD_SUBDIRECTORY( ${GTest_SOURCE_DIR} gtest )
        INCLUDE_DIRECTORIES( src ${GTest_SOURCE_DIR}/include ${GTest_SOURCE_DIR} )
        SET( GTEST_BOTH_LIBRARIES gtest gtest_main )
    ELSE( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "Windows" )
        FIND_PACKAGE( GTest )
        INCLUDE_DIRECTORIES( ${GTEST_INCLUDE_DIRS} )
    ENDIF( ${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "Windows" )
	
#TT: Install tests to dir. This allows them to be packed by CPack.
#Note: if your source tree is dirty (eg you've already run tests there and have output/ directories)
#those will also be installed too.

OPTION( INSTALL_TESTS "Install the tests into the nominated directory" OFF )
IF( INSTALL_TESTS )
    SET ( comma_CPP_TESTS_INSTALL_DIR bin CACHE PATH "Install directory for cpp unit tests" )
    SET ( comma_INSTALL_COMMA_TESTS_DIR share/test )
	
# Get the list of test directories (using glob).
FILE ( GLOB_RECURSE all_files 
		# RELATIVE is tricky. When you use it, you need to specify the path you expect the output to be RELATIVE to. (Yes, that path could be considerably non-relative to the file that is found.)
		RELATIVE ${CMAKE_SOURCE_DIR} 
		* )
		
SET ( test_dirs )
FOREACH ( i ${all_files} )
	if ( ${i} MATCHES "[\\/]test[\\/]" )
		LIST (APPEND test_dirs ${i} )
	endif ()
ENDFOREACH (i)

# Add those directories
	FOREACH (file ${test_dirs} )
			# Determine the parent directories for a given file so to prefix the install location (otherwise they just all get installed in the destination root)
			get_filename_component( dest_dir ${file} DIRECTORY )
                        STRING( REGEX MATCH ".*\\/output\\/.*" skip ${file} )
                        IF( NOT skip )
			# See that slash there? 		 vvv It's really important.
			INSTALL ( 	DIRECTORY ${dest_dir}/
						DESTINATION ${comma_INSTALL_COMMA_TESTS_DIR}/${CMAKE_PROJECT_NAME}/unit/${dest_dir} 
						USE_SOURCE_PERMISSIONS
                            PATTERN "output" EXCLUDE )
                        ENDIF( NOT skip )
	ENDFOREACH (file)

ENDIF( INSTALL_TESTS )
ENDIF( comma_BUILD_TESTS )

IF( comma_BUILD_ZEROMQ )
    FIND_LIBRARY( ZeroMQ_LIBRARY NAMES zmq )
    ADD_DEFINITIONS( -DUSE_ZEROMQ )
    INCLUDE_DIRECTORIES( ${ZeroMQ_INCLUDE_PATH} )
ENDIF( comma_BUILD_ZEROMQ )

SET( comma_ALL_LIBRARIES
        debug comma_application-d
        optimized comma_application
        debug comma_base-d
        optimized comma_base
        debug comma_csv-d
        optimized comma_csv
        debug comma_io-d
        optimized comma_io
        debug comma_string-d
        optimized comma_string
        debug comma_xpath-d
        optimized comma_xpath
        debug comma_name_value-d
        optimized comma_name_value )

IF(WIN32)
    SET( comma_ALL_EXTERNAL_LIBRARIES Ws2_32.lib )
ENDIF(WIN32)

IF(UNIX AND NOT QNXNTO AND NOT APPLE)
    SET( comma_ALL_EXTERNAL_LIBRARIES rt pthread )
ENDIF(UNIX AND NOT QNXNTO AND NOT APPLE)

SET( comma_ALL_EXTERNAL_LIBRARIES ${Boost_LIBRARIES} ${comma_ALL_EXTERNAL_LIBRARIES} )

IF( comma_BUILD_ZEROMQ )
    SET( comma_ALL_EXTERNAL_LIBRARIES ${comma_ALL_EXTERNAL_LIBRARIES} ${ZeroMQ_LIBRARY} )
ENDIF( comma_BUILD_ZEROMQ )

IF( BUILD_CPP_PYTHON_BINDINGS )
    SET( comma_ALL_EXTERNAL_LIBRARIES ${comma_ALL_EXTERNAL_LIBRARIES} ${PYTHON_LIBRARY} )
ENDIF( BUILD_CPP_PYTHON_BINDINGS )

SET( comma_ALL_LIBRARIES ${comma_ALL_LIBRARIES} ${comma_ALL_LIBRARIES} ${comma_ALL_EXTERNAL_LIBRARIES} ) # as gcc parses library list only once
SET( comma_EXTERNAL_INCLUDES ${Boost_INCLUDE_DIRS} )

CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

FILE( GLOB comma_PACKAGE_CMAKEFILES ${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles/*.cmake )

INCLUDE( ${CMAKE_CURRENT_SOURCE_DIR}/CMakeFiles/generate_${PROJECT_NAME}_config.cmake )

CONFIGURE_FILE( ${PROJECT_SOURCE_DIR}/CMakeFiles/use_${PROJECT_NAME}.cmake.in
                ${PROJECT_BINARY_DIR}/CMakeFiles/use_${PROJECT_NAME}.cmake COPYONLY IMMEDIATE )

SET( comma_PACKAGE_CMAKEFILES ${comma_PACKAGE_CMAKEFILES} ${PROJECT_BINARY_DIR}/CMakeFiles/use_${PROJECT_NAME}.cmake )
INSTALL( FILES ${comma_PACKAGE_CMAKEFILES}
         DESTINATION ${comma_INSTALL_PACKAGE_DIR} )

#-----------------------------------------------------------------------------
# add sources

ADD_SUBDIRECTORY( application )
ADD_SUBDIRECTORY( base )
ADD_SUBDIRECTORY( bash )
ADD_SUBDIRECTORY( containers )
ADD_SUBDIRECTORY( csv )
ADD_SUBDIRECTORY( dispatch )
ADD_SUBDIRECTORY( doc )
ADD_SUBDIRECTORY( io )
ADD_SUBDIRECTORY( math )
ADD_SUBDIRECTORY( name_value )
ADD_SUBDIRECTORY( packed )
ADD_SUBDIRECTORY( string )
ADD_SUBDIRECTORY( sync )
ADD_SUBDIRECTORY( util )
ADD_SUBDIRECTORY( visiting )
add_subdirectory( web )
if( comma_BUILD_XML )
    add_subdirectory( xml )
endif( comma_BUILD_XML )
add_subdirectory( xpath )

ADD_SUBDIRECTORY( etc/bash_completion.d )

ADD_SUBDIRECTORY( python )

#in future, may need to add third party dlls somehow, if they are linked as dlls. Not needed for statically linking to boost.
#if we set them up to install to bin they might be packaged through, e.g. roughly (using GLOB):
#install(FILES "${Boost_LIBRARY_DIRS}/*.dll" DESTINATION bin )

#the following set up optional parameters for cpack
SET(CPACK_PACKAGE_NAME "comma")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Command line utilities to manipulate various structured and unstructured data from offline or real time data.")
SET(CPACK_PACKAGE_VENDOR "Australian Centre for Field Robotics")
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
SET(CPACK_PACKAGE_VERSION_MAJOR "1")
SET(CPACK_PACKAGE_VERSION_MINOR ${COMMITDATE})
SET(CPACK_PACKAGE_VERSION_PATCH ${COMMITHASH})
SET(CPACK_PACKAGE_CONTACT "t.teo@acfr.usyd.edu.au")

# If the user builds shared, make sure that the RPM package (that might be generated) runs ldconfig when it's installed.
IF( BUILD_SHARED_LIBS )
	SET(CPACK_RPM_SPEC_MORE_DEFINE "%posttrans -p /sbin/ldconfig")
ENDIF( BUILD_SHARED_LIBS )


IF( BUILD_PYTHON_PACKAGES AND ADD_PYTHON_PACKAGES_TO_RPM )
    SET(CPACK_RPM_PACKAGE_REQUIRES "boost-thread recode socat psmisc python numpy")
    SET(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/python/comma-rpm-post-install.sh" )
    SET(CPACK_RPM_PRE_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/python/comma-rpm-pre-install.sh" )
	# Prevents /tmp from permission change when installing RPM package
	SET(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/tmp" )
ELSE( BUILD_PYTHON_PACKAGES AND ADD_PYTHON_PACKAGES_TO_RPM )
    SET(CPACK_RPM_PACKAGE_REQUIRES "boost-thread socat psmisc recode")
ENDIF( BUILD_PYTHON_PACKAGES AND ADD_PYTHON_PACKAGES_TO_RPM )


#this line finds the appropriate Visual Studio DLLS in windows. Maybe it would find other dlls if a different build tool were used. Probably should be windows only.
INCLUDE( InstallRequiredSystemLibraries )

#this line does all the work. Actually, even if this is the only line, you get all but the dlls...
INCLUDE( CPack )


