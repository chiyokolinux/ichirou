VERSION = 1.1

PREFIX =
MANPREFIX = $(PREFIX)/share/man

CC = gcc
LD = $(CC)
CPPFLAGS =
CFLAGS   = -Wextra -Wall -Os -s
LDFLAGS  = -s -static
