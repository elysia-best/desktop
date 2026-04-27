# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later
#
# keep the application name and short name the same or different for dev and prod build
# or some migration logic will behave differently for each build
if(OPENLIST_DEV)
    set( APPLICATION_NAME       "OpenListDev" )
    set( APPLICATION_SHORTNAME  "OpenListDev" )
    set( APPLICATION_EXECUTABLE "openlistdev" )
    set( APPLICATION_ICON_NAME  "OpenList" )
else()
    set( APPLICATION_NAME       "OpenList" )
    set( APPLICATION_SHORTNAME  "openlist" )
    set( APPLICATION_EXECUTABLE "openlist" )
    set( APPLICATION_ICON_NAME  "${APPLICATION_SHORTNAME}" )
endif()

set( APPLICATION_CONFIG_NAME "${APPLICATION_EXECUTABLE}" )
set( APPLICATION_DOMAIN     "oplist.org" )
set( APPLICATION_VENDOR     "OpenList Team" )
set( APPLICATION_UPDATE_URL "https://github.com/OpenListTeam/OpenList/releases" CACHE STRING "URL for updater" )
set( APPLICATION_HELP_URL   "https://doc.oplist.org" CACHE STRING "URL for the help menu" )

set( APPLICATION_ICON_SET   "SVG" )
set( APPLICATION_SERVER_URL "" CACHE STRING "URL for the server to use. If entered, the UI field will be pre-filled with it" )
set( APPLICATION_SERVER_URL_ENFORCE OFF ) # If set and APPLICATION_SERVER_URL is defined, the server can only connect to the pre-defined URL
set( APPLICATION_REV_DOMAIN "org.openlist.desktopclient" )
set( DEVELOPMENT_TEAM "" CACHE STRING "Apple Development Team ID" )
set( APPLICATION_VIRTUALFILE_SUFFIX "openlist" CACHE STRING "Virtual file suffix (not including the .)")
set( APPLICATION_OCSP_STAPLING_ENABLED OFF )
set( APPLICATION_FORBID_BAD_SSL OFF )

set( LINUX_PACKAGE_SHORTNAME "openlist" )
set( LINUX_APPLICATION_ID "${APPLICATION_REV_DOMAIN}.${LINUX_PACKAGE_SHORTNAME}")

set( THEME_CLASS            "OpenListTheme" )
set( WIN_SETUP_BITMAP_PATH  "${CMAKE_SOURCE_DIR}/admin/win/nsi" )

set( MAC_INSTALLER_BACKGROUND_FILE "${CMAKE_SOURCE_DIR}/admin/osx/installer-background.png" CACHE STRING "The MacOSX installer background image")

# set( THEME_INCLUDE          "${OEM_THEME_DIR}/mytheme.h" )
# set( APPLICATION_LICENSE    "${OEM_THEME_DIR}/license.txt )

## Updater options
option( BUILD_UPDATER "Build updater" ON )

option( WITH_PROVIDERS "Build with providers list" ON )

option( ENFORCE_VIRTUAL_FILES_SYNC_FOLDER "Enforce use of virtual files sync folder when available" OFF )
option( DISABLE_VIRTUAL_FILES_SYNC_FOLDER "Disable use of virtual files sync folder even when available" OFF )

option(ENFORCE_SINGLE_ACCOUNT "Enforce use of a single account in desktop client" OFF)

option( DO_NOT_USE_PROXY "Do not use system wide proxy, instead always do a direct connection to server" OFF )

option( WIN_DISABLE_USERNAME_PREFILL "Do not prefill the Windows user name when creating a new account" OFF )

## Theming options
set(OPENLIST_BACKGROUND_COLOR "#0078d4" CACHE STRING "Default OpenList background color")
set( APPLICATION_WIZARD_HEADER_BACKGROUND_COLOR ${OPENLIST_BACKGROUND_COLOR} CACHE STRING "Hex color of the wizard header background")
set( APPLICATION_WIZARD_HEADER_TITLE_COLOR "#ffffff" CACHE STRING "Hex color of the text in the wizard header")
option( APPLICATION_WIZARD_USE_CUSTOM_LOGO "Use the logo from ':/client/theme/colored/wizard_logo.(png|svg)' else the default application icon is used" ON )

#
## Windows Shell Extensions & MSI - IMPORTANT: Generate new GUIDs for custom builds with "guidgen" or "uuidgen"
#
if(WIN32)
    # Context Menu
    set( WIN_SHELLEXT_CONTEXT_MENU_GUID      "{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}" )

    # Overlays
    set( WIN_SHELLEXT_OVERLAY_GUID_ERROR     "{B1C2D3E4-F5A6-7890-BCDE-F01234567891}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_OK        "{C1D2E3F4-A5B6-7890-CDEF-012345678902}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_OK_SHARED "{D1E2F3A4-B5C6-7890-DEF0-123456789013}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_SYNC      "{E1F2A3B4-C5D6-7890-EF01-234567890124}" )
    set( WIN_SHELLEXT_OVERLAY_GUID_WARNING   "{F1A2B3C4-D5E6-7890-F012-345678901235}" )

    # MSI Upgrade Code (without brackets)
    set( WIN_MSI_UPGRADE_CODE                "A1B2C3D4-E5F6-7890-ABCD-EF1234567800" )

    # Windows build options
    option( BUILD_WIN_MSI "Build MSI scripts and helper DLL" OFF )
    option( BUILD_WIN_TOOLS "Build Win32 migration tools" OFF )
endif()

if (APPLE AND CMAKE_OSX_DEPLOYMENT_TARGET VERSION_GREATER_EQUAL 11.0)
    option( BUILD_FILE_PROVIDER_MODULE "Build the macOS virtual files File Provider module" OFF )
endif()
