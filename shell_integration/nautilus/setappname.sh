#!/bin/sh

# SPDX-FileCopyrightText: 2018 OpenList contributors
# SPDX-FileCopyrightText: 2015 ownCloud GmbH
# SPDX-License-Identifier: GPL-2.0-or-later

# this script replaces the line
#  appname = 'OpenList'
# with the correct branding name in the syncstate.py script
# It also replaces the occurrences in the class name so several
# branding can be loaded (see #6524)
sed -i.org -e "s/OpenList/$1/g" syncstate.py
