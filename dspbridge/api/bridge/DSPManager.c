/*
 * dspbridge/src/api/linux/DSPManager.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

/*
 *  ======== DSPManager.c ========
 *  Description:
 *      This is the source for the DSP/BIOS Bridge API manager module. The
 *      parameters are validated at the API level, but the bulk of the
 *      work is done at the driver level through the RM MGR module.
 *
 *  Public Functions:
 *      DSPManager_EnumNodeInfo
 *      DSPManager_EnumProcessorInfo
 *      DSPManager_Open
 *      DSPManager_Close
 *      DSPManager_WaitForEvents
 *
 *  OEM Functions:
 *      DSPManager_RegisterObject
 *      DSPManager_UnregisterObject
 *
 *! Revision History
 *! ================
 *! 07-Jul-2003 swa: Validate arguments in RegisterObject and UnregisterObject
 *! 15-Oct-2002 kc: Removed DSPManager_GetPerfData.
 *! 16-Aug-2002 map: Added DSPManager_RegisterObject/UnregisterObject
 *! 29-Nov-2000 rr: Use of DSP_ValidWritePtr. Code review changes incorporated.
 *! 22-Nov-2000 kc: Added DSPManager_GetPerfData().
 *! 25-Sep-2000 rr: Updated to Version 0.9
 *! 04-Aug-2000 rr: Name changed to DSPManager.c
 *! 20-Jul-2000 rr: Updated to Version 0.8
 *! 27-Jun-2000 rr: Modified to call into the Class driver.
 *! 12-Apr-2000 ww: Created based on DirectDSP API specification, Version 0.6.
 *
 */

/*  ----------------------------------- Host OS */
#include <host_os.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dbdefs.h>
#include <errbase.h>

/*  ----------------------------------- Trace & Debug */
#include <dbg.h>
#include <dbg_zones.h>

/*  ----------------------------------- Others */
#include <dsptrap.h>

/*  ----------------------------------- This */
#include "_dbdebug.h"
#include "_dbpriv.h"

#include <DSPManager.h>

#ifdef DEBUG_BRIDGE_PERF
#include <perfutils.h>
#endif

/*  ----------------------------------- Globals */
int hMediaFile = -1;		/* class driver handle */
static ULONG usage_count = 0;
static sem_t semOpenClose;
static bool bridge_sem_initialized = false;

/*  ----------------------------------- Definitions */
/* #define BRIDGE_DRIVER_NAME  "/dev/dspbridge"*/
#define BRIDGE_DRIVER_NAME  "/dev/DspBridge"
#define SYSFS_DRV_STATE		"/sys/devices/platform/C6410/drv_state"
#define ROOT_ACCESS		1406
#define RUNNING			0x2
#define DSP_HIB			0x9
#define RETENTION		0x8
#define MPU_HIB			0x7
/*
 *  ======== DspManager_Open ========
 *  Purpose:
 *      Open handle to the DSP/BIOS Bridge driver
 */
DBAPI DspManager_Open(UINT argc, PVOID argp)
{
	int status = 0;
	unsigned int drv_state;
	FILE *file;
	char line[4];
	int _Retry_;
	int retry;

	printf("[ALEX] DspManager_Open\n");
#if 0 //Commented for K32 DSP Bridge Driver
	if (argc == ROOT_ACCESS)
		goto open;

	_Retry_ = 5;
	while (_Retry_ > 0)
	{
	    file = fopen(SYSFS_DRV_STATE, "r");
	    if (!file)
		{
			printf ("[ALEX] File open Fail\n");
			//return DSP_EFAIL;
			//return 0x12121212;
			status = DSP_EFAIL +1;
		}
		else
		{
	        fgets(line, 3, file);
	        sscanf(line, "%d", &drv_state);
	        fclose(file);
	        if (drv_state != RUNNING && (drv_state > DSP_HIB || drv_state < MPU_HIB )) {
		        printf("Driver is not ready, access denied!\n");
				printf ("[ALEX] Driver is not ready, access denied![%d]\n", drv_state);
				//return DSP_EFAIL+1;
				//return 0x13131313;
				status = DSP_EFAIL + 2;
			}
			else
			{
				_Retry_ = 0;
				status = 0;
				goto open;
			}
	    }
		_Retry_--;
        usleep(100000);
	}

	if (status != 0);
		return status;
open:
#endif //K32 Bridge Driver

	if (!bridge_sem_initialized) {
		if (sem_init(&semOpenClose, 0, 1) == -1) {
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				 (TEXT("MGR: Failed to Initialize"
					   "the bridge semaphore\n")));
			return DSP_EFAIL+3;
		} else
			bridge_sem_initialized = true;
	}

	sem_wait(&semOpenClose);
	//if (usage_count == 0) {	/* try opening handle to Bridge driver */
	//	status = open(BRIDGE_DRIVER_NAME, O_RDWR);
	//	if (status >= 0)
	//		hMediaFile = status;
	//}
	
	if (usage_count == 0) { /* try opening handle to Bridge driver */
		//try a number of times, until dsp bridge node comes up:
		#define MAX_DSP_OPEN_RETRIES 100  //100 retries
		#define DSP_OPEN_TIMEOUT 100   //100 msec
		retry = MAX_DSP_OPEN_RETRIES;  //10 seconds
		do
		{
			status = open(BRIDGE_DRIVER_NAME, O_RDWR);
			retry--;
			if (status < 0) 
			usleep(DSP_OPEN_TIMEOUT * 1000);  //100 miliseconds
			else
			break;
 
		} while (retry >= 0);
		//done trying
	if (status >= 0)
		hMediaFile = status;
	}


	if (status >= 0) {
		/* Success in opening handle to Bridge driver */
		usage_count++;
		status = DSP_SOK;
	} 
    else
	{
		printf ("[ALEX] OPEN Fail[%d]\n", status);
		status = DSP_EFAIL+4;
	}


	/*printf ("argc = %d, hMediaFile[%x] = %d\n", argc, &hMediaFile,
					hMediaFile); */

	sem_post(&semOpenClose);

	return status;
}

/*
 *  ======== DspManager_Close ========
 *  Purpose:   Close handle to the DSP/BIOS Bridge driver
 */
DBAPI DspManager_Close(UINT argc, PVOID argp)
{
	int status = 0;

	sem_wait(&semOpenClose);

	if (usage_count == 1) {
		status = close(hMediaFile);
		if (status >= 0)
			hMediaFile = -1;
	}

	if (status >= 0) {
		/* Success in opening handle to Bridge driver */
		usage_count--;
		status = DSP_SOK;
	} else
		status = DSP_EFAIL;

	sem_post(&semOpenClose);

	/*printf ("close status = %d, hMediaFile[%x] = %d\n", status,
						&hMediaFile, hMediaFile); */

	return status;
}

/*
 *  ======== DSPManager_EnumNodeInfo ========
 *  Purpose:
 *      Enumerate and get configuration information about nodes configured
 *      in the node database.
 */
DBAPI DSPManager_EnumNodeInfo(UINT uNode, OUT struct DSP_NDBPROPS *pNDBProps,
			UINT uNDBPropsSize, OUT UINT *puNumNodes)
{
	DSP_STATUS status = DSP_SOK;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
		 (TEXT("MGR: DSPManager_EnumNodeInfo\r\n")));

	if (!DSP_ValidWritePtr(pNDBProps, sizeof(struct DSP_NDBPROPS)) &&
	    !DSP_ValidWritePtr(puNumNodes, sizeof(UINT))) {

		if (uNDBPropsSize >= sizeof(struct DSP_NDBPROPS)) {
			/* Set up the structure */
			/* Call DSP Trap */
			tempStruct.ARGS_MGR_ENUMNODE_INFO.uNode = uNode;
			tempStruct.ARGS_MGR_ENUMNODE_INFO.pNDBProps = pNDBProps;
			tempStruct.ARGS_MGR_ENUMNODE_INFO.uNDBPropsSize =
							uNDBPropsSize;
			tempStruct.ARGS_MGR_ENUMNODE_INFO.puNumNodes =
							puNumNodes;
			status = DSPTRAP_Trap(&tempStruct,
					CMD_MGR_ENUMNODE_INFO_OFFSET);
		} else {
			status = DSP_ESIZE;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				 (TEXT("MGR: pNDBProps is too Small \r\n")));
		}
	} else {
		/* Invalid pointer */
		status = DSP_EPOINTER;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			 (TEXT("MGR: pNDBProps is Invalid \r\n")));
	}

	return status;
}

/*
 *  ======== DSPManager_EnumProcessorInfo ========
 *  Purpose:
 *      Enumerate and get configuration information about available
 *      DSP processors.
 */
DBAPI DSPManager_EnumProcessorInfo(UINT uProcessor,
			     OUT struct DSP_PROCESSORINFO *pProcessorInfo,
			     UINT uProcessorInfoSize, OUT UINT *puNumProcs)
{
	DSP_STATUS status = DSP_SOK;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
		 (TEXT("MGR: DSPManager_EnumProcessorInfo\r\n")));

	if (!DSP_ValidWritePtr(pProcessorInfo, sizeof(struct DSP_PROCESSORINFO))
		&& !DSP_ValidWritePtr(puNumProcs, sizeof(UINT))) {

		if (uProcessorInfoSize >= sizeof(struct DSP_PROCESSORINFO)) {
			/* Call DSP Trap */
			tempStruct.ARGS_MGR_ENUMPROC_INFO.uProcessor =
								uProcessor;
			tempStruct.ARGS_MGR_ENUMPROC_INFO.pProcessorInfo =
								pProcessorInfo;
			tempStruct.ARGS_MGR_ENUMPROC_INFO.uProcessorInfoSize =
							uProcessorInfoSize;
			tempStruct.ARGS_MGR_ENUMPROC_INFO.puNumProcs =
								puNumProcs;

			status = DSPTRAP_Trap(&tempStruct,
				CMD_MGR_ENUMPROC_INFO_OFFSET);
		} else {
			status = DSP_ESIZE;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("MGR: uProcessorInfoSize is too Small \r\n")));
		}
	} else {
		/* Invalid pointer */
		status = DSP_EPOINTER;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			 (TEXT("MGR: pProcessorInfo is Invalid \r\n")));
	}

	return status;
}

/*
 *  ======== DSPManager_WaitForEvents ========
 *  Purpose:
 *      Block on Bridge event(s)
 */
DBAPI DSPManager_WaitForEvents(struct DSP_NOTIFICATION **aNotifications,
			 UINT uCount, OUT UINT *puIndex, UINT uTimeout)
{
	DSP_STATUS status = DSP_SOK;
	Trapped_Args tempStruct;

#if 0
	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
		 (TEXT("MGR: DSPManager_WaitForEvents\r\n")));
#endif

	if ((aNotifications) && (puIndex)) {

		if (uCount) {
			/* Set up the structure */
			/* Call DSP Trap */
			tempStruct.ARGS_MGR_WAIT.aNotifications =
							aNotifications;
			tempStruct.ARGS_MGR_WAIT.uCount = uCount;
			tempStruct.ARGS_MGR_WAIT.puIndex = puIndex;
			tempStruct.ARGS_MGR_WAIT.uTimeout = uTimeout;

			status = DSPTRAP_Trap(&tempStruct, CMD_MGR_WAIT_OFFSET);
		} else
			/* nStreams == 0 */
			*puIndex = (UINT) -1;

	} else
		/* Invalid pointer */
		status = DSP_EPOINTER;


	return status;
}

/*
 *  ======== DSPManager_RegisterObject ========
 *  Purpose:
 *  Register object with DCD module
 */
DBAPI DSPManager_RegisterObject(IN struct DSP_UUID *pUuid,
			  IN DSP_DCDOBJTYPE objType, IN CHAR *pszPathName)
{
	DSP_STATUS status = DSP_SOK;
	Trapped_Args tempStruct;
#ifdef DEBUG_BRIDGE_PERF
	struct timeval tv_beg;
	struct timeval tv_end;
	struct timezone tz;
	int timeRetVal = 0;

	timeRetVal = getTimeStamp(&tv_beg);
#endif

	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
		 (TEXT("MGR: DSPManager_RegisterObject\r\n")));

	if ((pUuid == NULL) || (objType > DSP_DCDDELETELIBTYPE) ||
	    (pszPathName == NULL)) {
		status = DSP_EINVALIDARG;
	}

	if (DSP_SUCCEEDED(status)) {
		/* Call DSP Trap */
		tempStruct.ARGS_MGR_REGISTEROBJECT.pUuid = pUuid;
		tempStruct.ARGS_MGR_REGISTEROBJECT.objType = objType;
		tempStruct.ARGS_MGR_REGISTEROBJECT.pszPathName = pszPathName;
		status = DSPTRAP_Trap(&tempStruct,
					CMD_MGR_REGISTEROBJECT_OFFSET);
	}
#ifdef DEBUG_BRIDGE_PERF
	timeRetVal = getTimeStamp(&tv_end);
	PrintStatistics(&tv_beg, &tv_end, "DSPManager_RegisterObject", 0);
#endif

	return status;
}

/*
 *  ======== DSPManager_UnregisterObject ========
 *  Purpose:
 *  Unregister object with DCD module
 */
DBAPI DSPManager_UnregisterObject(IN struct DSP_UUID *pUuid,
				IN DSP_DCDOBJTYPE objType)
{
	DSP_STATUS status = DSP_SOK;
	Trapped_Args tempStruct;
#ifdef DEBUG_BRIDGE_PERF
	struct timeval tv_beg;
	struct timeval tv_end;
	struct timezone tz;
	int timeRetVal = 0;

	timeRetVal = getTimeStamp(&tv_beg);
#endif

	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
		 (TEXT("MGR: DSPManager_RegisterObject\r\n")));

	if ((pUuid == NULL) || (objType > DSP_DCDDELETELIBTYPE))
		status = DSP_EINVALIDARG;


	if (DSP_SUCCEEDED(status)) {
		/* Call DSP Trap */
		tempStruct.ARGS_MGR_UNREGISTEROBJECT.pUuid = pUuid;
		tempStruct.ARGS_MGR_UNREGISTEROBJECT.objType = objType;
		status = DSPTRAP_Trap(&tempStruct,
				CMD_MGR_UNREGISTEROBJECT_OFFSET);
	}
#ifdef DEBUG_BRIDGE_PERF
	timeRetVal = getTimeStamp(&tv_end);
	PrintStatistics(&tv_beg, &tv_end, "DSPManager_UnregisterObject", 0);

#endif

	return status;
}

#ifndef RES_CLEANUP_DISABLE
/*
 *  ======== DSPManager_GetProcResourceInfo ========
 *  Purpose:
 *  Get GPP process resource info
 */
DBAPI DSPManager_GetProcResourceInfo(UINT *pBuf, UINT *pSize)
{
    DSP_STATUS      status = DSP_SOK;
    Trapped_Args    tempStruct;
    DEBUGMSG(DSPAPI_ZONE_FUNCTION,
	(TEXT("MGR: DSPManager_RegisterObject\r\n")));

	if (pBuf == NULL)
		status = DSP_EINVALIDARG;

	if (DSP_SUCCEEDED(status)) {
		/* Call DSP Trap */
		tempStruct.ARGS_PROC_GETTRACE.pBuf = (BYTE *)pBuf;
		status = DSPTRAP_Trap(&tempStruct, CMD_MGR_RESOUCES_OFFSET);
	}

    return status;
}
#endif

