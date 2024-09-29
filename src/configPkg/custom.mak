## THIS IS A GENERATED FILE -- DO NOT EDIT
.configuro: .libraries,rm4 linker.cmd package/cfg/Cornell_prm4.orm4

# To simplify configuro usage in makefiles:
#     o create a generic linker command file name 
#     o set modification times of compiler.opt* files to be greater than
#       or equal to the generated config header
#
linker.cmd: package/cfg/Cornell_prm4.xdl
	$(SED) 's"^\"\(package/cfg/Cornell_prm4cfg.cmd\)\"$""\"D:/ISC/Cornell/Cornell_FW/Cornell_V1/Build19/Build19/src/configPkg/\1\""' package/cfg/Cornell_prm4.xdl > $@
	-$(SETDATE) -r:max package/cfg/Cornell_prm4.h compiler.opt compiler.opt.defs
