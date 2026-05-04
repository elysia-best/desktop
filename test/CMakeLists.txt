# SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
# SPDX-FileCopyrightText: 2011 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

include(DefinePlatformDefaults)

include(openlist_add_test.cmake)

set(CMAKE_AUTOMOC TRUE)

find_package(Qt${QT_VERSION_MAJOR}Core5Compat ${REQUIRED_QT_VERSION} CONFIG QUIET)
find_package(Qt${QT_VERSION_MAJOR} OPTIONAL_COMPONENTS HttpServer CONFIG QUIET)

add_library(testutils
  STATIC
  syncenginetestutils.cpp
  themeutils.cpp
  testhelper.cpp
  sharetestutils.cpp
  endtoendtestutils.cpp
  activitylistmodeltestutils.cpp
  foldermantestutils.cpp
)

target_link_libraries(testutils PUBLIC OpenList::sync Qt::Test Qt::Core5Compat)
target_include_directories(testutils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(testutils PROPERTIES FOLDER Tests)

openlist_add_test(OpenListPropagator)
if(Qt${QT_VERSION_MAJOR}HttpServer_FOUND)
    target_compile_definitions(OpenListPropagatorTest PRIVATE HAVE_QHTTPSERVER=1)
    target_link_libraries(OpenListPropagatorTest PRIVATE Qt::HttpServer)
endif()

IF(BUILD_UPDATER)
    openlist_add_test(Updater)

    if(Qt${QT_VERSION_MAJOR}HttpServer_FOUND)
        target_compile_definitions(UpdaterTest PRIVATE HAVE_QHTTPSERVER=1)
        target_link_libraries(UpdaterTest PRIVATE Qt::HttpServer)
    endif()
endif()

openlist_add_test(NetrcParser)
openlist_add_test(OwnSql)
openlist_add_test(SyncJournalDB)
openlist_add_test(SyncFileItem)
openlist_add_test(ConcatUrl)
openlist_add_test(Cookies)
openlist_add_test(XmlParse)
openlist_add_test(ChecksumValidator)

if (NOT APPLE)
    openlist_add_test(ExcludedFiles)
endif()

openlist_add_test(Utility)

if (NOT APPLE)
    openlist_add_test(SyncEngine)
    openlist_add_test(SyncVirtualFiles)
endif()

openlist_add_test(SyncMove)
openlist_add_test(SyncDelete)
openlist_add_test(SyncConflict)
openlist_add_test(SyncFileStatusTracker)
openlist_add_test(Download)
openlist_add_test(ChunkingNg)
openlist_add_test(AsyncOp)
openlist_add_test(UploadReset)
openlist_add_test(AllFilesDeleted)
openlist_add_test(Blacklist)
openlist_add_test(LocalDiscovery)
openlist_add_test(RemoteDiscovery)

if (NOT APPLE)
    openlist_add_test(Permissions)
endif()

openlist_add_test(SelectiveSync)
openlist_add_test(DatabaseError)
openlist_add_test(LockedFiles)

if (NOT WIN32)
    openlist_add_test(FolderWatcher)
endif()

openlist_add_test(Capabilities)
openlist_add_test(Theme)
openlist_add_test(IconUtils)
openlist_add_test(UnifiedSearchListmodel)
openlist_add_test(ActivityListModel)
openlist_add_test(SortedActivityListModel)
openlist_add_test(ActivityData)
openlist_add_test(ShareModel)
openlist_add_test(ShareeModel)
openlist_add_test(SortedShareModel)
openlist_add_test(FileTagModel)
openlist_add_test(SyncConflictsModel)
openlist_add_test(DateFieldBackend)

openlist_add_test(FileSystem)

openlist_add_test(FolderStatusModel)


if( UNIX AND NOT APPLE )
    openlist_add_test(InotifyWatcher)
endif(UNIX AND NOT APPLE)

if (WIN32)
    include_directories(
        ${CMAKE_BINARY_DIR}/src/libsync/vfs/cfapi
    )

    openlist_add_test(SyncCfApi)
	openlist_add_test(CfApiShellExtensionsIPC)
    target_sources(CfApiShellExtensionsIPCTest PRIVATE "${CMAKE_SOURCE_DIR}/src/libsync/vfs/cfapi/shellext/thumbnailprovideripc.cpp" "${CMAKE_SOURCE_DIR}/src/libsync/vfs/cfapi/shellext/customstateprovideripc.cpp" "${CMAKE_SOURCE_DIR}/src/libsync/vfs/cfapi/shellext/ipccommon.cpp")
elseif(LINUX) # elseif(LINUX OR APPLE)
    openlist_add_test(SyncXAttr)
endif()

openlist_add_test(LongPath)
nextcloud_add_benchmark(LargeSync)

openlist_add_test(Account)
openlist_add_test(Folder)
openlist_add_test(FolderMan)
if(NOT BUILD_FILE_PROVIDER_MODULE)
    # the File Provider build crashes this test in CI for some reason
    # I'm not yet sure what's causing it as it works fine with a classic build...
    openlist_add_test(AccountSettings)
endif()

openlist_add_test(NetworkSettings)
openlist_add_test(UpdateChannel)
openlist_add_test(FileActionsModel)

configure_file(test_journal.db "${PROJECT_BINARY_DIR}/bin/test_journal.db" COPYONLY)

find_package(CMocka)
if (CMOCKA_FOUND)
    include(AddCMockaTest)
    add_subdirectory(csync)
endif (CMOCKA_FOUND)
