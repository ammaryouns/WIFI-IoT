# invoke SourceDir generated makefile for Cornell.prm4
Cornell.prm4: .libraries,Cornell.prm4
.libraries,Cornell.prm4: package/cfg/Cornell_prm4.xdl
	$(MAKE) -f D:\ISC\Cornell\Cornell_FW\Cornell_V1\Build19\Build19\src/src/makefile.libs

clean::
	$(MAKE) -f D:\ISC\Cornell\Cornell_FW\Cornell_V1\Build19\Build19\src/src/makefile.libs clean

