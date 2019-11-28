//#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define PROCNAME_CT         	"CameraTamperDetector"
#define LUMA_IPC_CTFILE       	"/var/LUMA_IPC_CTDATA"
#define PTZ_MOVE_SEC            20
#define STBLZ_WAITING_TIME      20
#define PTZ_LOCK_EXPIRY_TIME    1200              //20 min 

#define abs(x) 			(int)fabs(x)
#define UDP_LUMABUF_CT_SIZE	    20000
#define LUMA_EFFECTIVE_BIT	    14
#define EACHBYTE_BIT		    8

#define SIGPTZMOVE              SIGRTMIN+1                      //PTZ movement start
#define SIGVL                   SIGRTMIN+2                      // Video Lost happen
#define SIGVLEND                SIGRTMIN+3                      // Video Lost over
#define SIGFD                   SIGRTMIN+4                      // Frame Drop happen
#define SIGFDEND                SIGRTMIN+5                      // Frame Drop over
#define SIGAFOCUS               SIGRTMIN+6                      // Auto Focus
#define SIGFASST                SIGRTMIN+7                      //Focus Assist
#define SIGPTZMOVEEND           SIGRTMIN+8                      //PTZ movement End
#define SIGMDNIGHT				SIGRTMIN+9
#define SIGMDDAY				SIGRTMIN+10


#define CS_PTHREAD_EXIT							pthread_exit
#define CS_PTHREAD_CREATE 						pthread_create
#define CS_PTHREAD_ATTR_INT    						pthread_attr_init
#define CS_PTHREAD_ATTR_SETDETACHSTATE			pthread_attr_setdetachstate
#define CS_PTHREAD_ATTR_DESTROY					pthread_attr_destroy
#define CS_PTHREAD_DETACH					pthread_detach

struct ctlumaBuff{
      struct timeval TS;
      int bufLength;
      unsigned char lumaBuf[UDP_LUMABUF_CT_SIZE]; //Buffer for down sampled Luma data
} ctLumaBuff;

typedef struct ctMacroBlockSzTemplate 
{
    unsigned int col;
    unsigned int row;
    unsigned int macroBlkSz;
} ctMacroBlockSzTemplate;

static ctMacroBlockSzTemplate ctMacroBlockSzArr[] = 
{
    //{Col, Row, CT_MACROBLOCK_SZ}
    { 2560, 1920, 16 },
    { 1920, 1080, 12 },
    { 1536, 864, 12 },
    { 1472, 832, 16 },
    { 1280, 1024, 16 },		
    { 1280, 960, 16 },		
    { 1280, 800, 8 },		
    { 1280, 720, 8 },		
    { 1024, 576, 8 },		
    { 960, 544, 8 },
    { 768, 432, 8 },
    { 720, 576, 8 },
    { 720, 480, 8 },
    { 704, 480, 8 },
    { 704, 400, 8 },
    { 704, 576, 8 },
    { 700, 570, 8 },  // Not tested for resolution. Only used for initialization by luma_dispatch
    { 640, 368, 8 },   
    { 352, 240, 8 },
    { 352, 288, 8 },
    { 352, 208, 8 },
    { 320, 192, 8 },
    { 192, 112, 4 },
    { 160, 96, 4 },
/*  IMP!!!!!
Add the below unique rows by dividing resolution mentioned above by Macroblock size which will be used by md_region_create api
 since  the above portion will be used by MDmain.c to get the macroblock size and below table rows will used by API and will assign its 
macroblock size to 1 as we are already passing the downsampled data. We can seperate it to 2 tables later.
*/
    { 160, 120, 1 }, //Newly added for 2560x1920
    { 160, 90, 1 }, //for 1920x1080 
    { 128, 72, 1 }, //for 1536x864
    { 92, 52, 1 }, //for 1472x832 
    { 80, 64, 1 }, //for 1280x1024
    { 80, 60, 1 }, //for 1280x960
    { 160, 100, 1 }, //for 1280x800
    { 160, 90, 1 }, //for 1280x720
    { 128, 72, 1 }, //for 1024x576
    { 120, 68, 1 }, //for 960x544
    { 96, 54, 1 }, //for 768x432
    { 90, 72, 1 }, //for 720x576
    { 90, 60, 1 }, //for 720x480	
    { 88, 60, 1 }, //for 704x480
    { 88, 50, 1 }, //for 704x400
    { 88, 72, 1 }, //for 704x576
    { 87, 71, 1 }, //for 700x570
    { 80, 46, 1 }, //for 640x368
    { 44, 30, 1 }, //for 352x240	
    { 44, 36, 1 }, //for 352x288
    { 44, 26, 1 }, //for 352x208
    { 40, 24, 1 }, //for 320x192
	{ 48, 28, 1 }, //for 192x112
    { 40, 24, 1 } //for 160x96

};




