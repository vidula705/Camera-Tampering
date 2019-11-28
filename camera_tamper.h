#ifndef __CAMERA_TAMPER_H__
#define __CAMERA_TAMPER_H__

#ifdef __cplusplus
extern "C" {
#endif

void * init_tamper_detector( int width, int height, int channels);
int		ipf_tamper_processing( void *tHandle, unsigned char *inimg);
int 	deinit_tamper_detector(void *tHandle);

#ifdef __cplusplus
}
#endif

#endif
