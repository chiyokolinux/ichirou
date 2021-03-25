# this file is part of ichirou
# Copyright (c) 2020-2021 Emily <elishikawa@jagudev.net>
#
# ichirou is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ichirou is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ichirou.  If not, see <https://www.gnu.org/licenses/>.

VERSION = 1.10

PREFIX =
MANPREFIX = /usr/share/man

CC = gcc
LD = $(CC)
CPPFLAGS =
CFLAGS   = -Wextra -Wall -Os -s
LDFLAGS  = -s -static
