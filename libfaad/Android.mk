LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= bits.c \
   cfft.c      \
   decoder.c   \
   drc.c       \
   drm_dec.c   \
   error.c     \
   filtbank.c  \
   ic_predict.c    \
   is.c        \
   lt_predict.c    \
   mdct.c      \
   mp4.c       \
   ms.c        \
   output.c    \
   pns.c       \
   ps_dec.c    \
   ps_syntax.c     \
   pulse.c     \
   specrec.c   \
   syntax.c    \
   tns.c       \
   hcr.c       \
   huffman.c   \
   rvlc.c      \
   ssr.c       \
   ssr_fb.c    \
   ssr_ipqf.c  \
   common.c    \
   sbr_dct.c   \
   sbr_e_nf.c  \
   sbr_fbt.c   \
   sbr_hfadj.c     \
   sbr_hfgen.c     \
   sbr_huff.c  \
   sbr_qmf.c   \
   sbr_syntax.c    \
   sbr_tf_grid.c   \
   sbr_dec.c     \
   audio.c  \
   aac_decoding.c

LOCAL_MODULE:= libfaad

LOCAL_C_INCLUDES :=         \
    $(LOCAL_PATH)       \
    $(LOCAL_PATH)/include \
    $(FAAD2_TOP)/android    \
    $(FAAD2_TOP)/include    \
    $(LOCAL_PATH)/codebook

LOCAL_CFLAGS:=      \
    -DHAVE_CONFIG_H

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= aac_decoding.c 
LOCAL_MODULE:= aac2pcm

LOCAL_C_INCLUDES :=         \
    $(LOCAL_PATH)       \
    $(LOCAL_PATH)/include \
    $(FAAD2_TOP)/include    \

LOCAL_SHARED_LIBRARIES := \
	libfaad

include $(BUILD_EXECUTABLE)

