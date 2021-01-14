VERSION = 1.8

PREFIX = /usr
MANPREFIX = $(PREFIX)/share/man

CC = gcc
LD = $(CC)
CPPFLAGS =
CFLAGS   = -Wextra -Wall -Os -g # -s
LDFLAGS  = # -s # -static
