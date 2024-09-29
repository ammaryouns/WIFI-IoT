#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = C:/TI/simplelink_cc32xx_sdk_2_10_00_04/kernel/tirtos/packages;C:/TI/simplelink_cc32xx_sdk_2_10_00_04/source
override XDCROOT = c:/ti/xdctools_3_50_05_12_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = C:/TI/simplelink_cc32xx_sdk_2_10_00_04/kernel/tirtos/packages;C:/TI/simplelink_cc32xx_sdk_2_10_00_04/source;c:/ti/xdctools_3_50_05_12_core/packages;..
HOSTOS = Windows
endif
