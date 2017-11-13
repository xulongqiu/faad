LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= audio.c \
   				  main.c \
				  ../common/faad/aacinfo.c \
				  $(FAAD2_TOP)/common/faad/filestream.c \
				  $(FAAD2_TOP)/common/faad/getopt.c \
				  $(FAAD2_TOP)/common/faad/id3v2tag.c \
				  $(FAAD2_TOP)/common/mp4ff/mp4atom.c \
				  $(FAAD2_TOP)/common/mp4ff/mp4ff.c \
				  $(FAAD2_TOP)/common/mp4ff/mp4meta.c \
				  $(FAAD2_TOP)/common/mp4ff/mp4sample.c \
				  $(FAAD2_TOP)/common/mp4ff/mp4tagupdate.c \
				  $(FAAD2_TOP)/common/mp4ff/mp4util.c

LOCAL_MODULE:= faad

LOCAL_C_INCLUDES :=         \
    $(LOCAL_PATH)       \
    $(FAAD2_TOP)/common/faad \
    $(FAAD2_TOP)/common/mp4ff \
    $(FAAD2_TOP)/include    \
    $(LOCAL_PATH)/codebook

LOCAL_CFLAGS:=      \
    -DHAVE_CONFIG_H

LOCAL_SHARED_LIBRARIES := \
	libfaad

include $(BUILD_EXECUTABLE)
