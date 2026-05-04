# SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2014 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later
project(libsync)
find_package(KF6Archive REQUIRED)
include(DefinePlatformDefaults)

set(CMAKE_AUTOMOC TRUE)

if ( APPLE )
    list(APPEND OS_SPECIFIC_LINK_LIBRARIES
         /System/Library/Frameworks/CoreServices.framework
         /System/Library/Frameworks/Foundation.framework
         /System/Library/Frameworks/AppKit.framework
    )
endif()

if (WIN32)
  list(APPEND OS_SPECIFIC_LINK_LIBRARIES
    Crypt32
  )
endif()

set(libsync_SRCS
    account.h
    account.cpp
    wordlist.h
    wordlist.cpp
    bandwidthmanager.h
    bandwidthmanager.cpp
    capabilities.h
    capabilities.cpp
    clientproxy.h
    clientproxy.cpp
    cookiejar.h
    cookiejar.cpp
    discovery.h
    discovery.cpp
    discoveryphase.h
    discoveryphase.cpp
    filesystem.h
    filesystem.cpp
    helpers.cpp
    httplogger.h
    httplogger.cpp
    updatechannel.h
    updatechannel.cpp
    logger.h
    logger.cpp
    accessmanager.h
    accessmanager.cpp
    configfile.h
    configfile.cpp
    abstractnetworkjob.h
    abstractnetworkjob.cpp
    networkjobs.h
    networkjobs.cpp
    iconjob.h
    iconjob.cpp
    owncloudpropagator.h
    owncloudpropagator.cpp
    openlisttheme.h
    openlisttheme.cpp
    openlistsettings.h
    openlistsettings.cpp
    userstatusconnector.h
    userstatusconnector.cpp
    deletejob.h
    deletejob.cpp
    progressdispatcher.h
    progressdispatcher.cpp
    propagatorjobs.h
    propagatorjobs.cpp
    propagatedownload.h
    propagatedownload.cpp
    propagateupload.h
    propagateupload.cpp
    propagateuploadv1.cpp
    propagateuploadng.cpp
    bulkpropagatorjob.h
    bulkpropagatorjob.cpp
    bulkpropagatordownloadjob.h
    bulkpropagatordownloadjob.cpp
    putmultifilejob.h
    putmultifilejob.cpp
    propagateremotedelete.h
    propagateremotedelete.cpp
    propagateremotemove.h
    propagateremotemove.cpp
    propagateremotemkdir.h
    propagateremotemkdir.cpp
    syncengine.h
    syncengine.cpp
    syncfileitem.h
    syncfileitem.cpp
    syncfilestatustracker.h
    syncfilestatustracker.cpp
    localdiscoverytracker.h
    localdiscoverytracker.cpp
    syncresult.h
    syncresult.cpp
    syncoptions.h
    syncoptions.cpp
    theme.h
    theme.cpp
    datetimeprovider.h
    datetimeprovider.cpp
    creds/dummycredentials.h
    creds/dummycredentials.cpp
    creds/abstractcredentials.h
    creds/abstractcredentials.cpp
    creds/credentialscommon.h
    creds/credentialscommon.cpp
    creds/keychainchunk.h
    creds/keychainchunk.cpp
    creds/openlistcredentials.h
    creds/openlistcredentials.cpp
    caseclashconflictsolver.h
    caseclashconflictsolver.cpp
)

if (WIN32)
    # to fix warnings from ntstatus.h
    add_definitions(-DUMDF_USING_NTSTATUS)
endif()

if(TOKEN_AUTH_ONLY)
    set (libsync_SRCS
        ${libsync_SRCS}
        creds/tokencredentials.h
        creds/tokencredentials.cpp)
else()
    set (libsync_SRCS
        ${libsync_SRCS}
        creds/httpcredentials.h
        creds/httpcredentials.cpp)
endif()

# These headers are installed for libowncloudsync to be used by 3rd party apps
set(owncloudsync_HEADERS
    account.h
    syncengine.h
    configfile.h
    networkjobs.h
    progressdispatcher.h
    syncfileitem.h
    syncresult.h
)

set(creds_HEADERS
    creds/abstractcredentials.h
    creds/openlistcredentials.h
    creds/httpcredentials.h
)

IF (NOT APPLE)
    INSTALL(
        FILES ${owncloudsync_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/openlistsync/mirall
    )
    INSTALL(
        FILES ${creds_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/openlistsync/creds
    )
ENDIF(NOT APPLE)

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS WebSockets Xml Sql Gui Svg Widgets)

add_library(openlistsync SHARED ${libsync_SRCS})
add_library(OpenList::sync ALIAS openlistsync)

target_link_libraries(openlistsync
  PUBLIC
  OpenList::csync
  OpenSSL::Crypto
  OpenSSL::SSL
  PkgConfig::OPENSC-LIBP11
  ${OS_SPECIFIC_LINK_LIBRARIES}
  Qt::Core
  Qt::Network
  Qt::WebSockets
  Qt::Xml
  Qt::Sql
  Qt::Gui
  Qt::Svg
  Qt::Widgets
  KF6::Archive
  Qt::Core5Compat
)

target_compile_features(openlistsync
    PRIVATE
        cxx_std_17
)

target_compile_definitions(openlistsync PRIVATE OPENSSL_SUPPRESS_DEPRECATED)

if (NOT TOKEN_AUTH_ONLY)
    find_package(Qt${QT_MAJOR_VERSION} COMPONENTS REQUIRED Widgets Svg)
    target_link_libraries(openlistsync PUBLIC Qt::Widgets Qt::Svg Qt${QT_MAJOR_VERSION}Keychain::Qt${QT_MAJOR_VERSION}Keychain)
endif()

if(Inotify_FOUND)
    target_include_directories(openlistsync PRIVATE ${Inotify_INCLUDE_DIRS})
    target_link_libraries(openlistsync PUBLIC ${Inotify_LIBRARIES})
endif()

GENERATE_EXPORT_HEADER( openlistsync
	BASE_NAME openlistsync
	EXPORT_MACRO_NAME OWNCLOUDSYNC_EXPORT
        EXPORT_FILE_NAME owncloudlib.h
	STATIC_DEFINE OWNCLOUD_BUILT_AS_STATIC
)

target_include_directories(openlistsync PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(
  openlistsync
    PROPERTIES
      VERSION
        ${MIRALL_VERSION}
      SOVERSION
        ${MIRALL_SOVERSION}
      RUNTIME_OUTPUT_DIRECTORY
        ${BIN_OUTPUT_DIRECTORY}
      LIBRARY_OUTPUT_NAME
        ${APPLICATION_EXECUTABLE}sync
      RUNTIME_OUTPUT_NAME
        ${APPLICATION_EXECUTABLE}sync
      ARCHIVE_OUTPUT_NAME
        ${APPLICATION_EXECUTABLE}sync
)

if(BUILD_OWNCLOUD_OSX_BUNDLE)
    # macdeployqt (POST_BUILD on nextcloud) replaces the build-tree RPATH with
    # @loader_path/../Frameworks.  Setting BUILD_WITH_INSTALL_RPATH prevents CMake
    # from trying to adjust RPATHs at cmake --install time (which would conflict
    # with changes macdeployqt has already made).
    #
    # INSTALL_NAME_DIR "@rpath" keeps the dylib's install_name as
    # @rpath/libopenlistsync.0.dylib through cmake --install.  Without it CMake
    # defaults to ${CMAKE_INSTALL_PREFIX}/lib/libopenlistsync.0.dylib and rewrites
    # the nextcloud executable's @rpath reference to that absolute path during
    # install -- a path nothing is ever installed to -- causing dyld to fail at
    # launch with "Library not loaded: .../build/.../lib/libopenlistsync.0.dylib".
    set_target_properties(openlistsync PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_NAME_DIR "@rpath"
    )
endif()

if(NOT BUILD_OWNCLOUD_OSX_BUNDLE)
    install(TARGETS openlistsync
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
else()
    install(TARGETS openlistsync DESTINATION ${OWNCLOUD_OSX_BUNDLE}/Contents/MacOS)
endif()

if (WIN32)
    install(FILES $<TARGET_PDB_FILE:openlistsync> DESTINATION bin OPTIONAL)
endif()

add_subdirectory(vfs)
