#------------------------------------------------------------------------------
# Copyright (c) 2014-2015 Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from this
# software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#------------------------------------------------------------------------------

# Common definitions
CC       = gcc
PRJCFLAGS	= -g
LD	= gcc
LDFLAGS	=
AR	= ar
ARFLAGS	=
OBJ_DIR	= obj
RANLIB	= ranlib
RM	= rm
ECHO	= echo
INSTALL	= install

ifeq ($(BOOTROM_SRCDIR),)
BOOTROM_SRCDIR = $(HOME)/work/bootrom
endif

ODIR         = obj
COMMONOBJDIR = $(TOPDIR)/obj
BINDIR       = $(TOPDIR)/bin
IDIR         = $(TOPDIR)/include
LIBDIR       = $(TOPDIR)/libs
SCRIPTDIR	 = $(TOPDIR)/scripts
INSTALLDIR	 ?= /usr/local

SHARED_INCDIR = $(BOOTROM_SRCDIR)/common/shared_inc
COMMON_INCDIRS = -I$(TOPDIR)/include -I$(SHARED_INCDIR)

INSTALL_BINS = create-tftf create-ffff display-tftf display-ffff
INSTALL_BINS += s2config sign-tftf
INSTALL_SCRIPTS = nuttx2ffff

$(ODIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC_DIRS) -MM -MT $@ -MF $(patsubst %.o,%.d,$@) -c $<
	$(CC) -c -o $@ $< $(CFLAGS) $(INC_DIRS)


