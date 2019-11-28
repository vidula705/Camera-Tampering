#+++++++++++++++++++++++++++++++++++++++++++
#	Author:         Aries
#	Creation Date:  2007/10/31 05:10 PM
#  Description:
#			 Makefile of Camera Tamper Detector.
#+++++++++++++++++++++++++++++++++++++++++++
SRCS := CTmain.c camera_tamper.c
OBJS := CTmain.o camera_tamper.o
EXE := CameraTamperDetector
CFLAGS := -O2 -I$(GEMTEK_DIR)/sysutil/include -I$(GEMTEK_DIR)/hd/include -Wall -I$(GEMTEK_DIR)/SecureStorage  -I$(GEMTEK_DIR)/CameraControls -I$(GEMTEK_DIR)/mbcodec/include
SDK_FLAGS = -I$(PROJ_ROOT)/src/cisco/sdk/api -L$(PROJ_ROOT)/src/cisco/sdk/libraries -lsdk

.PHONY:sdk

ALL : CTmain

#CTmain : $(mainSRC) MD.h
CTmain : $(SRCS) 
#$(CC) $(CFLAGS) $(mainSRC) -o $(EXE) -lpthread -L$(LD_LIBRARY_PATH) -lhdcs -ldspal -lhdal -lhdrl -L$(GEMTEK_DIR)/cdp/lib -lcdpdclient -lcdpdquery -L$(GEMTEK_DIR)/cdp/lib -L$(GEMTEK_DIR)/nvram/bin -lnvram -L$(OPENSOURCE_DIR)/pcre/.libs -lpcre -L$(GEMTEK_DIR)/sysutil -lsysutil -lusb -lusb-1.0 -L$(GEMTEK_DIR)/tidsputil -ltidsputil -L$(GEMTEK_DIR)/UserStatus -lUserStatus -L$(GEMTEK_DIR)/libstore/lib -lstore -L$(GEMTEK_DIR)/SecureStorage -lSecureStorage -L$(OPENSSL_DIR) -lcrypto -L$(GEMTEK_DIR)/apputil -lapputil -L$(GEMTEK_DIR)/LogServer/msqlog -lmsglog -L$(GEMTEK_DIR)/ctrlServer/ctrlMsgQ -lctrlmq -L$(GEMTEK_DIR)/ctrlServer/ctrlMsgQ -lctrlmq
	$(CC) $(CFLAGS) -c $(SRCS)
	$(CC) $(CFLAGS) $(OBJS) -o $(EXE) -lpthread -L$(LD_LIBRARY_PATH) -lhdcs -ldspal -lhdal -lhdrl -Xlinker --start-group -L$(CISCO_DIR)/utils/env -lenv -L$(GEMTEK_DIR)/nvram/bin -lnvram -L$(GEMTEK_DIR)/SecureStorage -lSecureStorage -L$(OPENSSL_DIR) -lcrypto -lssl -Xlinker --end-group -L$(OPENSOURCE_DIR)/pcre/.libs -lpcre -L$(GEMTEK_DIR)/sysutil -lsysutil -lusb -lusb-1.0 -L$(GEMTEK_DIR)/tidsputil -ltidsputil -L$(GEMTEK_DIR)/UserStatus -lUserStatus -L$(GEMTEK_DIR)/apputil -lapputil -L$(GEMTEK_DIR)/LogServer/msqlog -lmsglog -L$(GEMTEK_DIR)/ctrlServer/ctrlMsgQ -lctrlmq -L$(GEMTEK_DIR)/libstore/lib -lstore -lm -L$(PROJ_ROOT)/src/linux/targets/dm368_fs/lib/ -L$(GEMTEK_DIR)/ApplicationManager/rel/appmgr/lib/ -lappmgrclient -L$(GEMTEK_DIR)/mbstream/storage-lib -lstorage
	-install -m 755 $(EXE) $(RAMDISK_ROOT)/bin

sdk:
	$(MAKE) -C $(PROJ_ROOT)/src/cisco/sdk libraries/libsdk.so

clean:
	-rm -f $(EXE) *.o core
