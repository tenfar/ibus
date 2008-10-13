# vim:set noet ts=4:
#
# ibus - The Input Bus
#
# Copyright (c) 2007-2008 Huang Peng <shawn.p.huang@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place, Suite 330,
# Boston, MA  02111-1307  USA

INCLUDES = \
	-I$(top_srcdir) \
	$(NULL)

bin_PROGRAMS = ibus-daemon
ibus_daemon_SOURCES = \
	main.c \
	server.c \
	server.h \
	connection.c \
	connection.h \
	$(NULL)
ibus_daemon_CFLAGS = \
	@X11_CFLAGS@ \
	@GLIB2_CFLAGS@ \
	@DBUS_CFLAGS@ \
	-DG_LOG_DOMAIN=\"IBUS\" \
	-I$(top_srcdir)/src \
	$(NULL)
ibus_daemon_LDFLAGS = \
	@X11_LIBS@ \
	@GLIB2_LIBS@ \
	@DBUS_LIBS@ \
	$(top_builddir)/src/libibus.la \
	$(NULL)
