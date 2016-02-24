/***************************************************************************************************
 * Author : Rishi Gupta
 *
 * This file is part of 'serial communication manager' library.
 *
 * The 'serial communication manager' is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * The 'serial communication manager' is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with serial communication manager. If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined (__linux__) || defined (__APPLE__)
#include <errno.h>
#include <unistd.h>
#endif

#include <jni.h>
#include "CP210xManufacturing.h"
#include "scm_cp210xmanufacturing.h"

/*
 * Prints fatal error on console. Java application can deploy a Java level framework which redirects
 * data for STDERR to a log file.
 */
int LOGE(const char *msg_a, const char *msg_b) {
	fprintf(stderr, "%s , %s\n", msg_a, msg_b);
	fflush(stderr);
	return 0;
}

int LOGEN(const char *msg_a, const char *msg_b, unsigned int error_num) {
	fprintf(stderr, "%s , %s , error code : %d\n", msg_a, msg_b, error_num);
	fflush(stderr);
	return 0;
}


/*
 * For C-standard/POSIX/OS specific/Custom/JNI errors, this function is called. It sets a pointer which is checked
 * by java method when native function returns. If the pointer is set exception of class as set by this function is thrown.
 *
 * The type 1 indicates standard (C-standard/POSIX/OS specific) error, 2 indicate custom (defined by this library) error,
 * 3 indicates custom error with message string.
 */
void throw_serialcom_exception(JNIEnv *env, int type, int error_code, const char *msg) {
	jint ret = 0;
	char buffer[256];
	jclass serialComExceptionClass = NULL;
#if _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600 && ! _GNU_SOURCE
#else
	char *error_msg = NULL;
#endif
	(*env)->ExceptionClear(env);
	serialComExceptionClass = (*env)->FindClass(env, SCOMEXPCLASS);
	if((serialComExceptionClass == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		(*env)->ExceptionClear(env);
		LOGE(E_FINDCLASSSCOMEXPSTR, FAILTHOWEXP);
		return;
	}

	if(type == 1) {
		/* Caller has given posix/os-standard error code, get error message corresponding to this code. */
		/* This need to be made more portable to remove compiler specific dependency */
#if _POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600 && ! _GNU_SOURCE
		memset(buffer, '\0', sizeof(buffer));
		errno = 0;
		ret = strerror_r(error_code, buffer, 256);
		if(ret < 0) {
			LOGEN(FAILTHOWEXP, "strerror_r", error_code);
		}
		ret = (*env)->ThrowNew(env, serialComExceptionClass, buffer);
		if(ret < 0) {
			LOGE(FAILTHOWEXP, buffer);
		}
#else
		error_msg = strerror_r(error_code, buffer, 256);
		if(error_msg == NULL) {
			ret = (*env)->ThrowNew(env, serialComExceptionClass, buffer);
			if(ret < 0) {
				LOGE(FAILTHOWEXP);
			}
		}else {
			ret = (*env)->ThrowNew(env, serialComExceptionClass, error_msg);
			if(ret < 0) {
				LOGE(FAILTHOWEXP);
			}
		}
#endif
	}else if(type == 2) {
		/* Caller has given custom error code, need to get exception message corresponding to this code. */
		memset(buffer, '\0', sizeof(buffer));
		switch (error_code) {
		case CP210x_DEVICE_NOT_FOUND: strcpy(buffer, "CP210x_DEVICE_NOT_FOUND");
		break;
		case CP210x_INVALID_HANDLE: strcpy(buffer, "CP210x_INVALID_HANDLE");
		break;
		case CP210x_INVALID_PARAMETER: strcpy(buffer, "CP210x_INVALID_PARAMETER");
		break;
		case CP210x_DEVICE_IO_FAILED: strcpy(buffer, "CP210x_DEVICE_IO_FAILED");
		break;
		case CP210x_FUNCTION_NOT_SUPPORTED: strcpy(buffer, "CP210x_FUNCTION_NOT_SUPPORTED");
		break;
		case CP210x_GLOBAL_DATA_ERROR: strcpy(buffer, "CP210x_GLOBAL_DATA_ERROR");
		break;
		case CP210x_FILE_ERROR: strcpy(buffer, "CP210x_FILE_ERROR");
		break;
		case CP210x_COMMAND_FAILED: strcpy(buffer, "CP210x_COMMAND_FAILED");
		break;
		case CP210x_INVALID_ACCESS_TYPE: strcpy(buffer, "CP210x_INVALID_ACCESS_TYPE");
		break;
		default : strcpy(buffer, E_UNKNOWN);
		}
		ret = (*env)->ThrowNew(env, serialComExceptionClass, buffer);
		if(ret < 0) {
			LOGE(FAILTHOWEXP, buffer);
		}
	}else {
		/* Caller has given exception message explicitly */
		ret = (*env)->ThrowNew(env, serialComExceptionClass, msg);
		if(ret < 0) {
			LOGE(FAILTHOWEXP, msg);
		}
	}
}
