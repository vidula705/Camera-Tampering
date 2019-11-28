#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <time.h>
#include <asm/types.h>
#include <fcntl.h>	
#include <pthread.h>
#include <poll.h>
#include "../nvram/include/nvram.h"
#include "../nvram/include/Token.h"
#include "csbase.h"
#include "alapi.h"
#include "CT.h"
#include "../EventManager/EventMngrDef.h"
#include "camera_tamper.h"

int width, height,ctHeight,ctWidth,LUMABUF_SIZE;
int PTZ_PatrolLock, PTZ_MoveLock, isSigKillRecvd=0;
int readSettingFlag;
static time_t resChgInitTime, ptzLockInitTime;
static bool g_resChgFlag=false;

#define CS_LOG
#define CT_MACROBLOCK_SZ    		24 //Considering default MacroBlk size as 24
#define POLL_TIMEOUT_MSECS  		35 //Since we receive the luma data at 30fps
#define CHECK_TAMPER_ALERT_SEC  	5 // Check tamper alerts after 60 sec

void gracefulShut(int sig)
{
    printf("Camera Tamper cleanup\n");
    isSigKillRecvd = true;
}

//================================================================
void kickoffStbzPeriod()
{
    g_resChgFlag = true;
    resChgInitTime = CS_TIME(NULL);
}

/*****************************************************/
/*  sleep several seconds wait for PTZ Move complete */
/*****************************************************/
void *PTZActTimerThread(void *arg){
    //        MD_DEBUG("\n enter PTZActTimerThread() sleep %d sec \n  enter PTZActTimerThread() sleep \n",PTZ_MOVE_SEC );    
    CS_SLEEP(PTZ_MOVE_SEC);
    PTZ_MoveLock = false;
    //        MD_DEBUG("\n run pthread_exit()\n");
    CS_PTHREAD_EXIT("");
}

/*******************************************/
/*  set PTZ_PatrolLock to lock or unlock     */
/*******************************************/
void setPTZPatrolLock(int sig)
{
    //       MD_DEBUG("\n\n enter setPTZPatrolLock PTZ_PatrolLock = %s \n enter setPTZPatrolLock PTZ_PatrolLock  \n\n",PTZ_PatrolLock ?"true":"flase");
    PTZ_PatrolLock = !(PTZ_PatrolLock);
}

/*********************************/
/*  set PTZ_MoveLock to lock     */
/*********************************/
void setPTZMoveLock(int sig)
{
    //pthread_t a_thread;
    CS_LOG("##-Begin PTZmove lock -##\n");
	
    PTZ_MoveLock = true;
	ptzLockInitTime = CS_TIME(NULL);
	CS_LOG("###PTZMove lock signal received###\n");
	
    /*CS_PTHREAD_CREATE(&a_thread,(void *)CS_NULL,PTZActTimerThread,(void *)CS_NULL);
      CS_PTHREAD_DETACH(a_thread);*/
}

/*********************************/
/*  set PTZ_MoveUnLock to unlock     */
/*********************************/
void setPTZMoveUnLock(int sig)
{  
    CS_LOG("@@-Begin PTZ Unlock-@@\n");
    PTZ_MoveLock = false;
	CS_LOG("@@@PTZMove Unlock signal received@@@\n");
	/* 
        Kick off the stabilization period which will ignore the events for 20 sec
        Assumtion: The Motor should finish the movement (reaching the position) before 20 sec.
        Else if user moves the camera very slowly it may cause false MD events because 
        we will unlock once we issue the Goto Preset/AbsPositon command to motor
       */
	kickoffStbzPeriod();
}

//================================================================
void sig_readSetting(void) 
{
    readSettingFlag = true;
}

//Setup action Handler for PTZSignals received from PTZdaemon
void setPTZSignalHandlers() {

	struct sigaction setup_action;
	sigset_t block_mask;
	
    sigemptyset(&block_mask);
	sigaddset (&block_mask, SIGPTZMOVEEND);
	setup_action.sa_handler = setPTZMoveLock;
    setup_action.sa_mask = block_mask;
    setup_action.sa_flags = 0;
    sigaction (SIGPTZMOVE, &setup_action, NULL);

	
	sigemptyset(&block_mask);
	sigaddset (&block_mask, SIGPTZMOVE);
	setup_action.sa_handler = setPTZMoveUnLock;
	setup_action.sa_mask = block_mask;
	setup_action.sa_flags = 0;
	sigaction (SIGPTZMOVEEND, &setup_action, NULL);

}
//================================================================
int getMacroBlkSize(int width, int height)
{   
    unsigned int ctMacroBlkSz,i;
    unsigned int macroBlkArrSz;

    /* Initialize md macro blk size to default */
    ctMacroBlkSz = CT_MACROBLOCK_SZ;

    macroBlkArrSz= sizeof(ctMacroBlockSzArr) / sizeof(ctMacroBlockSzTemplate);
    /* Loop to use md macro block size based on resolution */
    for (i = 0; i < macroBlkArrSz; i++)
    {
	if ((height == ctMacroBlockSzArr[i].row)&&(width == ctMacroBlockSzArr[i].col)){
	    ctMacroBlkSz = ctMacroBlockSzArr[i].macroBlkSz;
	    break;
	}
    }
    if(i==macroBlkArrSz) {
	printf("initSetting Error: Couldm't find the CT macro block size for the configured resolution\n");
    }
    return ctMacroBlkSz;	 

}

//================================================================
void initSetting(void) {
    int widthS =-1,heightS=-1;
    unsigned int ctMacroBlkSz;

    if(CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry0.VideoSetting.enabled"))) {
	widthS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry0.VideoSetting.videoResolutionWidth"));
	heightS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry0.VideoSetting.videoResolutionHeight"));
    }
    else if(CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry1.VideoSetting.enabled"))) {
	widthS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry1.VideoSetting.videoResolutionWidth"));
	heightS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry1.VideoSetting.videoResolutionHeight"));
    }

    width = widthS;
    height = heightS;

    ctMacroBlkSz = getMacroBlkSize(widthS,heightS);

    ctHeight = heightS/ctMacroBlkSz;
    ctWidth = widthS/ctMacroBlkSz;
    LUMABUF_SIZE=ctHeight*ctWidth;
    printf("initSetting:Initial Luma Buffer size is:[%d]\n",LUMABUF_SIZE);

}

//================================================================
void readSetting()
{
    int widthS = -1, heightS = -1;
    unsigned int ctMacroBlkSz;
    printf("In readSetting\n"); 
    
    if(CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry0.VideoSetting.enabled"))) {
	widthS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry0.VideoSetting.videoResolutionWidth"));
	heightS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry0.VideoSetting.videoResolutionHeight"));
    }
    else if(CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry1.VideoSetting.enabled"))) {
	widthS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry1.VideoSetting.videoResolutionWidth"));
	heightS = CS_ATOI(nvram_safe_get("StreamingSetting.StreamingChannelList.StreamingChannelEntry1.VideoSetting.videoResolutionHeight"));
    }
    if(widthS==-1&&heightS==-1) {
		return;
    }
    //Detect the resolution change and delete old MDconfig
    if(widthS!=width || heightS!=height) {
	printf("Change in resolution\n");
	//kickoff stablization period  so that we will discard the luma data for configured period
	kickoffStbzPeriod();
    }

    width = widthS;
    height = heightS;
    ctMacroBlkSz = getMacroBlkSize(widthS,heightS);

    ctHeight = heightS/ctMacroBlkSz;
    ctWidth = widthS/ctMacroBlkSz;
    printf("readSetting: ctWidth:[%d]  ctHeight:[%d]\n",ctWidth,ctHeight);

    LUMABUF_SIZE = ctHeight*ctWidth; //Use the ctHeight and ctWidth as the data is already downsampled at capture based on the MacroBolcksize
    printf("Updating the Luma buffer size :%d\n",LUMABUF_SIZE);

}


/************************************/
/*     CAMERA TAMPER DETECTION FUNCTION    */
/************************************/
int main(int argc, char *argv[]) 
{	
    int sockfd;
    struct sockaddr_un  SockAddr,clntAddr;
    int clntLen ,CurrentTime ;// , iRegionIdx ;
    char aLine[CMD_LENGTH];
   // unsigned char  *iBuf;
    struct ctlumaBuff lb;
	void *Tamperhandle;
	int tamper_status = 0, tamper_flag = 0, tamper_count = 0;
	STamperSocketData TamperSocketData;
	struct timeval prevTime, currTime;
 	
    int count,ret;
    unsigned long int expiringThresholdAt,currentTime;
    struct timeval tempTime;
    struct pollfd pfd[1];
    
    if (Set_Pid(PROCNAME_CT) == PID_NOT_FOUND) {
	CS_SPRINTF(aLine,"Application %s set pid error !  program exit ! \n ",PROCNAME_CT);
	MD_DEBUG((aLine));  
	CS_SYSLOG(LOG_ERR,aLine);
	return (PID_NOT_FOUND);
    }
    
	initSetting();	
	
	PTZ_MoveLock = false;
    PTZ_PatrolLock = false;
	readSettingFlag = false;
	g_resChgFlag = true;
	
	CS_SIGNAL(SIGUSR1,sig_readSetting);
	CS_SIGNAL(SIGUSR2,setPTZPatrolLock);
	
    //setup signal Handlers for PTZ recevied from PTZ daemon for any PTZ operations
    setPTZSignalHandlers();	
    CS_SIGNAL(SIGKILL,gracefulShut);CS_SIGNAL(SIGTERM,gracefulShut);
    CS_UNLINK(LUMA_IPC_CTFILE);
	
	CS_MEMSET(&TamperSocketData,CS_NULL,sizeof(TamperSocketData));
	
	// initialize tamper detector
	Tamperhandle = (void *)CT_Init( ctWidth, ctHeight, 1);
	
    sockfd = CS_SOCKET(PF_UNIX,SOCK_DGRAM,INITIAL_VALUE);
    //Set socket to NON_BLOCKING
    int flags = fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flags|O_NONBLOCK);

    CS_MEMSET(&SockAddr,CS_NULL,sizeof(SockAddr)); 
    SockAddr.sun_family = AF_UNIX;
    CS_STRCPY(SockAddr.sun_path,LUMA_IPC_CTFILE);
    CS_BIND(sockfd,(struct sockaddr *)&SockAddr,sizeof(SockAddr));   

	resChgInitTime = CS_TIME(NULL);	
	gettimeofday(&prevTime, NULL);
 
    while(true) 
    { 
		if(readSettingFlag || isSigKillRecvd) {		
			CS_LOG("READSETTING!!!!!!!!\n");
			//CS_SYSTEM("killall -USR2 Event_Manager");
			if(isSigKillRecvd) {
				printf("SignalRecvd:Exiting after sending the MD STOP alarms if any pending\n");
				break;
			}
			readSetting();
			readSettingFlag = false;			
			//Deinit camera tamper algorithm
			if(!CT_Deinit(Tamperhandle)) {
				printf("Re-initialized tamper detector \n");
				// re-initialize tamper detector with new resolution
				Tamperhandle = (void *)CT_Init( ctWidth, ctHeight, 1);
			}			
        }
	
		//Initialize the poll and call poll
		pfd[0].fd=sockfd;
		pfd[0].events = POLLIN;

		ret= poll(pfd,1,POLL_TIMEOUT_MSECS);

		if(ret<=0) {
			continue;
		}
		CS_LOG("Poll notifies Input socket has data \n");

		//recv LUMA 		    	
		count=CS_RECVFROM(sockfd,&lb,sizeof(struct ctlumaBuff),CS_NULL,(struct sockaddr *)&clntAddr,&clntLen);

		if(lb.bufLength<LUMABUF_SIZE) 
			continue;
		//printf("After Recv Lumadata size:[%d]\n",lb.bufLength);
		
		//Check if there is PTZlock for very long time(PTZ lock expiry time) and unlock if expired
		if(PTZ_MoveLock == true) {
			CurrentTime = CS_TIME(NULL);
			if (CS_DIFFTIME(CurrentTime,ptzLockInitTime) > PTZ_LOCK_EXPIRY_TIME) {
				setPTZMoveUnLock(0);   
			} else {
				continue;
			}
		}
		
		if(PTZ_PatrolLock == true) { 
			continue;
		}
	
		if (g_resChgFlag==true) {
			CurrentTime = CS_TIME(NULL);
			CS_LOG("Checking the Resolution change period\n");
			if (CS_DIFFTIME(CurrentTime,resChgInitTime) > STBLZ_WAITING_TIME) {
			g_resChgFlag=false;			
			}
			else {	
			continue;	
			}
		}
		
		// Send scaled Luma (Y) data to camera tampering alogrithm
		tamper_status = CT_ProcessFrame(Tamperhandle, lb.lumaBuf);	
		//printf( "\t\t Tamper status =%d\n",tamper_status);
		//sendSocketData_noBlock((char*)&TamperSocketData,TAMPER,sizeof(TamperSocketData));
		
		if(tamper_status == TAMPER_ALERT) {
			
            /* Check tamper_flag to avoid sending continuous tamper alerts */
			if(!tamper_flag) {
				TamperSocketData.status = tamper_status;
				sendSocketData_noBlock((char*)&TamperSocketData,TAMPER,sizeof(TamperSocketData));
				tamper_flag = 1;
				printf("------------- CAMERA TAMPERED -------------\n");
				gettimeofday(&prevTime, NULL);
			}
			else{
				gettimeofday(&currTime, NULL);
				if((currTime.tv_sec - prevTime.tv_sec) >= CHECK_TAMPER_ALERT_SEC) {
					printf("------------- RESET FIELD OF VIEW -------- \n");
					CT_ResetFOV(Tamperhandle, lb.lumaBuf);
					gettimeofday(&prevTime, NULL);
				}
			}
		}
		else {
		//	printf("\t\t No tamper detected \n");
			tamper_flag = 0;
			//TamperSocketData.status = tamper_status;
			//sendSocketData_noBlock((char*)&TamperSocketData,TAMPER_END ,sizeof(TamperSocketData));
		}
		

    }
    
	//Deinit camera tamper algorithm
	CT_Deinit(Tamperhandle);
	
    CS_CLOSE(sockfd);
    CS_UNLINK(LUMA_IPC_CTFILE);
    return RC_OK;	
}   
