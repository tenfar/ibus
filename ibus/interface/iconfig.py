# vim:set et sts=4 sw=4:
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

__all__ = ("IConfig", )

import dbus.service
from ibus.common import \
    IBUS_CONFIG_IFACE

class IConfig (dbus.service.Object):
    # define method decorator.
    method = lambda **args: \
        dbus.service.method (dbus_interface = IBUS_CONFIG_IFACE, \
            **args)

    # define signal decorator.
    signal = lambda **args: \
        dbus.service.signal (dbus_interface = IBUS_CONFIG_IFACE, \
            **args)

    # define async method decorator.
    async_method = lambda **args: \
        dbus.service.method (dbus_interface = IBUS_CONFIG_IFACE, \
            async_callbacks = ("reply_cb", "error_cb"), \
            **args)

    @method (in_signature = "ss", out_signature = "s")
    def GetString (self, key, default_value):
        pass

    @method (in_signature = "si", out_signature = "i")
    def GetInt (self, key, default_value):
        pass

    @method (in_signature = "sb", out_signature = "b")
    def GetBool (self, key, default_value):
        pass

    @method (in_signature = "ss")
    def SetString (self, key, value):
        pass

    @method (in_signature = "si")
    def SetInt (self, key, value):
        pass

    @method (in_signature = "sb")
    def SetBool (self, key, value):
        pass

    @signal (signature = "sv")
    def ValueChanged (self, key, value):
        pass
