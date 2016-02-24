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

/* - This file contains native code to communicate with tty-style port in Unix-like operating systems.
 * - When printing error number, number returned by OS is printed as it is.
 * - There will be only one instance of this shared library at runtime. So if something goes wrong
 *   it will affect everything, until this library has been unloaded and then loaded again.
 * - Wherever possible avoid JNI data types.
 * - Sometimes, the JNI does not like some pointer arithmetic so it is avoided wherever possible. */

#if defined (__linux__) || defined (__APPLE__) || defined (__SunOS) || defined(__sun) || defined(__FreeBSD__) \
		|| defined(__OpenBSD__) || defined(__NetBSD__) || defined(__hpux__) || defined(_AIX)

/* Make primitives such as read and write resume, in case they are interrupted by signal,
 * before they actually start reading or writing data. The partial success case are handled
 * at appropriate places in functions applicable.
 * For details see features.h about MACROS defined below. */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* C */
#include <stdarg.h>      /* ISO C Standard. Variable arguments  */
#include <stdio.h>       /* ISO C99 Standard: Input/output      */
#include <stdlib.h>      /* Standard ANSI routines              */
#include <string.h>      /* String function definitions         */
#include <errno.h>       /* Error number definitions            */

/* Unix */
#include <unistd.h>      /* UNIX standard function definitions  */
#include <fcntl.h>       /* File control definitions            */
#include <dirent.h>      /* Format of directory entries         */
#include <sys/types.h>   /* Primitive System Data Types         */
#include <sys/stat.h>    /* Defines the structure of the data   */
#include <pthread.h>     /* POSIX thread definitions            */
#include <sys/param.h>
#include <sys/select.h>

#if defined (__linux__)
#include <linux/types.h>
#include <linux/termios.h>  /* POSIX terminal control definitions for Linux (termios2) */
#include <linux/serial.h>
#include <linux/ioctl.h>
#include <sys/eventfd.h>    /* Linux eventfd for event notification. */
#include <signal.h>
#include <sys/uio.h>
#include <regex.h>
#include <stddef.h>         /* Avoid OS/lib implementation specific dependencies. */
#include <libudev.h>
#include <time.h>
#include <stdint.h>
#endif

#if defined (__APPLE__)
#include <termios.h>
#include <sys/ioctl.h>
#include <paths.h>
#include <sysexits.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <IOKit/serial/ioss.h>
#include <IOKit/IOBSD.h>
#endif

#if defined (__SunOS)
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#endif

/* jni_md.h contains the machine-dependent typedefs for data types. Instruct compiler to include it. */
#include <jni.h>
#include "unix_like_serial_lib.h"

/* Common interface with java layer for supported OS types. */
#include "../../com_embeddedunveiled_serial_internal_SerialComPortJNIBridge.h"

#undef  UART_NATIVE_LIB_VERSION
#define UART_NATIVE_LIB_VERSION "1.0.4"

/* This is the maximum number of threads and hence data listeners instance we support. */
#define MAX_NUM_THREADS 1024

/* Reference to JVM shared among all the threads. */
JavaVM *jvm;

/* When creating data looper threads, we pass some data to thread. A index in this array,
 * holds pointer to the structure which is passed as parameter to a thread. Every time a
 * data looper thread is created, we save the location of parameters passed to it and update
 * the index to be used next time.
 *
 * This array is protected by mutex locks.
 * Functions creating data/event threads write/modify data in this array.
 * Functions destroying data/event thread delete/modify data in this array.
 * Functions in thread or the thread itself only read data in this array. */
int dtp_index = 0;
struct com_thread_params fd_looper_info[MAX_NUM_THREADS] = { {0} };

/* Used to protect global data from concurrent access. */
#if defined (__linux__)
pthread_mutex_t mutex = {{0}};
#else
pthread_mutex_t mutex = {0};
#endif

/* Holds information for USB hot plug monitor facility. */
int usb_dev_monitor_index = 0;
struct usb_dev_monitor_info usb_hotplug_monitor_info[MAX_NUM_THREADS] = { { 0 } };

/* For Solaris, we maintain an array which will list all ports that have been opened. Now if
 * somebody tries to open already opened port claiming to be exclusive owner, we will deny the
 * request, except for root user. */
#ifdef __SunOS
struct port_name_owner opened_ports_list[MAX_NUM_THREADS] = { {0} };
#endif

/* Kept for Debugging and testing only. We do not want to use any JNI/JVM/JAVA specific mechanism.
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *pvt) {
}
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *pvt) {
}
__attribute__((constructor)) static void init_scmlib() {
}
 */

/* Release mutex when library is un-loaded. */
__attribute__((destructor)) static void exit_scmcomlib() {
	pthread_mutex_destroy(&mutex);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    initNativeLib
 * Signature: ()I
 *
 * This function gets the JVM interface (used in the Invocation API) associated with the current
 * thread and save it so that it can be used across native library, threads etc. It creates and
 * prepares mutex object to synchronize access to global data. Clear all exceptions and prepares
 * SerialComException class for exception throwing.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_initNativeLib(JNIEnv *env, 
		jobject obj) {

	int ret = 0;
	jclass serialComExceptionClass = NULL;

	ret = (*env)->GetJavaVM(env, &jvm);
	if(ret < 0) {
		serialComExceptionClass = (*env)->FindClass(env, SCOMEXPCLASS);
		if((serialComExceptionClass == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
			(*env)->ExceptionClear(env);
			LOGE(E_FINDCLASSSCOMEXPSTR, "NATIVE initNativeLib() could not get JVM.");
			return -1;
		}
		(*env)->ThrowNew(env, serialComExceptionClass, E_GETJVMSTR);
		return -1;
	}

	ret = pthread_mutex_init(&mutex, NULL);
	if(ret != 0) {
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	/* clear if something unexpected was there. */
	if((*env)->ExceptionCheck(env) == JNI_TRUE) {
		(*env)->ExceptionClear(env);
	}
	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getNativeLibraryVersion
 * Signature: ()Ljava/lang/String;
 *
 * Returns native library version from hard-coded string or NULL if an error occurs.
 *
 * @return version string on success otherwise NULL if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jstring JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getNativeLibraryVersion(JNIEnv *env,
		jobject obj) {
	jstring version = NULL;
	version = (*env)->NewStringUTF(env, UART_NATIVE_LIB_VERSION);
	if((version == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_NEWSTRUTFSTR);
		return NULL;
	}
	return version;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    listAvailableComPorts
 * Signature: ()[Ljava/lang/String;
 *
 * Use OS specific way to detect/identify serial ports known to system at the instant this
 * function is called. Do not try to open any port as for bluetooth this may result in system
 * trying to make BT connection and failing with time out.
 *
 * @return array of serial ports found in system on success otherwise NULL if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jobjectArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_listAvailableComPorts(JNIEnv *env,
		jobject obj) {
	return listAvailableComPorts(env);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    listUSBdevicesWithInfo
 * Signature: (I)[Ljava/lang/String;
 *
 * Find USB devices with information about them using platform specific facilities. The info sequence is :
 * Vendor ID, Product ID, Serial number, Product, Manufacturer, USB bus number, USB Device number.
 *
 * @return array of Strings containing info about USB device(s) otherwise NULL if an error occurs or no
 *         devices found.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jobjectArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_listUSBdevicesWithInfo(JNIEnv *env,
		jobject obj, jint vendorFilter) {
	return list_usb_devices(env, vendorFilter);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    findComPortFromUSBAttributes
 * Signature: (IILjava/lang/String;)[Ljava/lang/String;
 *
 * Find the COM Port/ device node assigned to USB-UART converter device using platform specific
 * facilities.
 *
 * @return array of Strings containing com ports if found matching given criteria otherwise NULL if
 *         error occurs or no node matching criteria is found.
 * @throws SerialComException if any JNI function, system call or C function fails.

 */
JNIEXPORT jobjectArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_findComPortFromUSBAttributes(JNIEnv *env,
		jobject obj, jint vid, jint pid, jstring serial) {
	return vcp_node_from_usb_attributes(env, vid, pid, serial);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    isUSBDevConnected
 * Signature: (IILjava/lang/String;)I
 *
 * Enumerate and check if given usb device identified by its USB-IF VID and PID is connected to
 * system or not.
 *
 * @return 1 if device is connected, 0 if not connected , -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_isUSBDevConnected(JNIEnv *env,
		jobject obj, jint vid, jint pid, jstring serialNumber) {
	return is_usb_dev_connected(env, vid, pid, serialNumber);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    openComPort
 * Signature: (Ljava/lang/String;ZZZ)J
 *
 * Open and initialize the port because 'termios' settings persist even if port has been closed.
 * We set default settings as; non-canonical mode, 9600 8N1 with no time out and no delay.
 * The terminal settings set here, are to operate in raw-like mode (no characters interpreted).
 * Note that all the bit mask may have been defined using OCTAL representation of number system.
 *
 * @return file descriptor number on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jlong JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_openComPort(JNIEnv *env,
		jobject obj, jstring portName, jboolean enableRead, 
		jboolean enableWrite, jboolean exclusiveOwner) {

	int ret = -1;
	jlong fd = -1;
	int OPEN_MODE = -1;
	int n = 0;
	const char* portpath = NULL;

	portpath = (*env)->GetStringUTFChars(env, portName, NULL);
	if((portpath == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_GETSTRUTFCHARSTR);
		return -1;
	}

	if((enableRead == JNI_TRUE) && (enableWrite == JNI_TRUE)) {
		OPEN_MODE = O_RDWR;
	}else if(enableRead == JNI_TRUE) {
		OPEN_MODE = O_RDONLY;
	}else if(enableWrite == JNI_TRUE) {
		OPEN_MODE = O_WRONLY;
	}else {
	}

	/* Don't become controlling terminal and do not wait for DCD line to be enabled from other end. */
	errno = 0;
	fd = open(portpath, OPEN_MODE | O_NDELAY | O_NOCTTY);
	if(fd < 0) {
		(*env)->ReleaseStringUTFChars(env, portName, portpath);
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	(*env)->ReleaseStringUTFChars(env, portName, portpath);

	/* Enable blocking I/O behavior. Control behavior through VMIN and VTIME. */
	n = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, n & ~O_NDELAY);

	/* Make the caller, exclusive owner of this port. This will prevent additional opens except by root-owned process. */
	if(exclusiveOwner == JNI_TRUE) {
#if defined (__linux__) || defined (__APPLE__)
		errno = 0;
		ret = ioctl(fd, TIOCEXCL);
		if(ret < 0) {
			close(fd);
			throw_serialcom_exception(env, 1, errno, NULL);
			return -1;
		}
#elif defined (__SunOS)
		/* Exclusive ownership is not supported for Solaris as of now. */
		close(fd);
		return -241;
#endif
	}

	return fd;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    closeComPort
 * Signature: (J)I
 *
 * Free the file descriptor for reuse and tell kernel to free up structures associated with this file.
 * In scenarios like if the port has been removed from the system physically or tty structures have
 * been de-allocated etc. we proceed to close ignoring some errors.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_closeComPort(JNIEnv *env, 
		jobject obj, jlong fd) {

	int ret = -1;
	int exit_loop = 0;

	/* Flush data (if any due to any reason) to the receiver. */
	tcdrain(fd);

	/* Failing disclaiming exclusive ownership of port will produce unexpected results if same port is to be used by more users.
	 * So if we fail we return with error and user application should retry closing. */
#if defined (__linux__) || defined (__APPLE__)
	errno = 0;
	ret = ioctl(fd, TIOCNXCL);
	if(ret < 0) {
		if((errno == ENXIO) || (errno == ENOTTY) || (errno == EBADF) || (errno == ENODEV)) {
			/* ignore */
		}else {
			throw_serialcom_exception(env, 1, errno, NULL);
			return -1;
		}

	}
#endif

	/* Whether we were able to flush remaining data or not, we proceed to close port. */
	do {
		errno = 0;
		ret = close(fd);
		if(ret < 0) {
			if(errno == EINTR) {
				errno = 0;
				continue;
			}else if((errno == ENXIO) || (errno == ENOTTY) || (errno == EBADF) || (errno == ENODEV)) {
			}else {
				throw_serialcom_exception(env, 1, errno, NULL);
				return -1;
			}
		}
		exit_loop = 1;
	}while (exit_loop == 0);

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    readBytes
 * Signature: (JI)[B
 *
 * The maximum number of bytes that read system call can read is the value that can be 
 * stored in an object of type ssize_t. In JNI programming 'jbyte' is 'signed char'. 
 * Default number of bytes to read is set to 1024 in java layer.
 *
 * 1. If data is read from serial port and no error occurs, return array of bytes.
 * 2. If there is no data to read from serial port and no error occurs, return NULL.
 * 3. If error occurs for whatever reason, return NULL and throw exception.
 *
 * The number of bytes return can be less than the request number of bytes but can never be 
 * greater than the requested number of bytes. This is implemented using total_read variable. 
 * Size request should not be more than 2048.
 *
 * This function do not block any signals and handles the following scenarios :
 * 1. Complete read in 1st pass itself.
 * 2. Partial read followed by complete read.
 * 3. Partial read followed by partial read then complete read.
 *
 * @return data byte buffer containing data bytes read from serial port or NULL if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jbyteArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_readBytes(JNIEnv *env,
		jobject obj, jlong fd, jint count) {

	int i = -1;
	int index = 0;
	int partial_data = -1;
	int num_bytes_to_read = 0;
	int total_read = 0;         /* track total number of bytes read till now */
	ssize_t ret = -1;
	jbyte buffer[2 * 1024];
	jbyte final_buf[3 * 1024];  /* Sufficient enough to deal with consecutive multiple partial reads. */
	jbyteArray dataRead;

	num_bytes_to_read = count; /* initial value */
	do {
		if(partial_data == 1) {
			num_bytes_to_read = count - total_read;
		}

		errno = 0;
		ret = read(fd, buffer, num_bytes_to_read);

		if((ret > 0) && (errno == 0)) {
			total_read = total_read + ret;
			/* This indicates we got success and have read data. */
			/* If there is partial data read previously, append this data. */
			if(partial_data == 1) {
				for(i = 0; i < ret; i++) {
					final_buf[index] = buffer[i];
					index++;
				}
				/* Pass the final fully successful read to java layer. */
				dataRead = (*env)->NewByteArray(env, index);
				if((dataRead == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
					throw_serialcom_exception(env, 3, 0, E_NEWBYTEARRAYSTR);
					return NULL;
				}
				(*env)->SetByteArrayRegion(env, dataRead, 0, index, final_buf);
				if((*env)->ExceptionOccurred(env)) {
					throw_serialcom_exception(env, 3, 0, E_SETBYTEARRREGIONSTR);
					return NULL;
				}
				return dataRead;
			}else {
				/* Pass the successful read to java layer straight away. */
				dataRead = (*env)->NewByteArray(env, ret);
				if((dataRead == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
					throw_serialcom_exception(env, 3, 0, E_NEWBYTEARRAYSTR);
					return NULL;
				}
				(*env)->SetByteArrayRegion(env, dataRead, 0, ret, buffer);
				if((*env)->ExceptionOccurred(env)) {
					throw_serialcom_exception(env, 3, 0, E_SETBYTEARRREGIONSTR);
					return NULL;
				}
				return dataRead;
			}
		}else if((ret > 0) && (errno == EINTR)) {
			total_read = total_read + ret;
			/* This indicates, there is data to read, however, we got interrupted before we finish reading
			 * all of the available data. So we need to save this partial data and get back to read remaining. */
			for(i = 0; i < ret; i++) {
				final_buf[index] = buffer[i];
				index++;
			}
			partial_data = 1;
			errno = 0;
			continue;
		}else if(ret < 0) {
			if(errno == EINTR) {
				/* This indicates that we should retry as we are just interrupted by a signal. */
				errno = 0;
				continue;
			}else {
				/* This indicates, irrespective of, there was data to read or not, we got an error
				 * during operation. */
				throw_serialcom_exception(env, 1, errno, NULL);
				return NULL;
			}
		}else if(ret == 0) {
			/* This indicates, no data on port, EOF or port is removed from system (may be). */
			return NULL;
		}else {
			/* do nothing, relax :) */
		}
	} while(1);

	return NULL;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    readBytes
 * Signature: (J[BIIJ)I
 *
 * Read data bytes from serial port and places into given buffer. The number of bytes to read i.e.
 * length must not be greater than 2048 bytes.
 *
 * If the context specifies blocking I/O, it will block until data is available at serial port or
 * will not block if context specifies non-blocking I/O.
 *
 * If the blocked I/O read operation is unblocked, it throws SerialComManager.EXP_UNBLOCKIO exception.
 *
 * @return number of data bytes read from serial port or -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_readBytes__J_3BII(JNIEnv *env,
		jobject obj, jlong fd, jbyteArray buffer, jint offset, jint length, jlong context) {

	ssize_t ret = -1;
	int result = 0;
	fd_set fds;
	int try_reading_data = 0;
	jbyte buf[2 * 1024];

	if(context == -1) {
		/* non-blocking read operation needed */
		try_reading_data = 1;

	}else {
		/* blocking read operation needed */

#if defined (__linux__)
		FD_ZERO(&fds);
		FD_SET((int)context, &fds);
		FD_SET(fd, &fds);
		errno = 0;
		result = pselect((MAX(fd, (int)context) + 1), &fds, NULL, NULL, NULL, NULL);
#endif
#if defined (__APPLE__)
		FD_ZERO(&fds);
		FD_SET((int) context[0], &fds);
		FD_SET(fd, &fds);
		errno = 0;
		result = pselect((MAX(fd, ((int) context[0])) + 1), &fds, NULL, NULL, NULL, NULL);
#endif
		if(result < 0) {
			throw_serialcom_exception(env, 1, errno, NULL);
			return -1;
		}

		/* check if we should just come out waiting state and return to caller. if yes, throw
		 * exception with message that will be identified by application to understand that blocked
		 * I/O has been unblocked. */
#if defined (__linux__)
		if((result > 0) && FD_ISSET((int)context, &fds)) {
			throw_serialcom_exception(env, 3, 0, E_UNBLOCKIO);
			return -1;
		}
#endif
#if defined (__APPLE__)
		if((result > 0) && FD_ISSET((int)context[0], &fds)) {
			throw_serialcom_exception(env, 3, 0, E_UNBLOCKIO);
			return -1;
		}
#endif

		if((result > 0) && FD_ISSET(fd, &fds)) {
			try_reading_data = 1;
		}
	}

	if(try_reading_data == 1) {
		do {
			errno = 0;
			ret = read(fd, buf, length);
			if(ret > 0) {
				/* copy data from native buffer to Java buffer. */
				(*env)->SetByteArrayRegion(env, buffer, (jsize)offset, (jsize)ret, buf);
				if((*env)->ExceptionOccurred(env) != NULL) {
					throw_serialcom_exception(env, 3, 0, E_SETBYTEARRREGIONSTR);
					return -1;
				}
				return (jint)ret;
			}else if(ret < 0) {
				if(errno == EINTR) {
					/* This indicates that we should retry as we are just interrupted by a signal. */
					errno = 0;
					continue;
				}else {
					/* This indicates, irrespective of, there was data to read or not, we got an error
					 * during operation. */
					throw_serialcom_exception(env, 1, errno, NULL);
					return -1;
				}
			}else {
				return 0;
			}
		}while (1);
	}

	/* should not be reached */
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    readBytesBlocking
 * Signature: (JIJ)[B
 *
 * Blocks on :
 * (1) serial port file descriptor to become available for reading.
 * (2) event file descriptor that is used to come out of blocking state.
 *
 * If the blocked I/O read operation is unblocked, it throws SerialComManager.EXP_UNBLOCKIO exception.
 *
 * @return data byte buffer containing data bytes read from serial port or NULL if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jbyteArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_readBytesBlocking(JNIEnv *env,
		jobject obj, jlong fd, jint count, jlong context) {

	int result = 0;
	fd_set fds;
	int i = -1;
	int index = 0;
	int partial_data = -1;
	int num_bytes_to_read = 0;
	int total_read = 0;
	ssize_t ret = -1;
	jbyte buffer[2 * 1024];
	jbyte final_buf[3 * 1024];
	jbyteArray dataRead;

	/* prepare to block */
#if defined (__linux__)
	FD_ZERO(&fds);
	FD_SET((int)context, &fds);
	FD_SET(fd, &fds);
	errno = 0;
	result = pselect((MAX(fd, (int)context) + 1), &fds, NULL, NULL, NULL, NULL);
#endif
#if defined (__APPLE__)
	FD_ZERO(&fds);
	FD_SET((int) context[0], &fds);
	FD_SET(fd, &fds);
	errno = 0;
	result = pselect((MAX(fd, ((int) context[0])) + 1), &fds, NULL, NULL, NULL, NULL);
#endif
	if(result < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return NULL;
	}

	/* check if we should just come out waiting state and return to caller. if yes, throw exception with
	 * message that will be identified by application to understand that blocked I/O has been unblocked. */
#if defined (__linux__)
		if((result > 0) && FD_ISSET((int)context, &fds)) {
			throw_serialcom_exception(env, 3, 0, E_UNBLOCKIO);
			return NULL;
		}
#endif
#if defined (__APPLE__)
		if((result > 0) && FD_ISSET((int)context[0], &fds)) {
			throw_serialcom_exception(env, 3, 0, E_UNBLOCKIO);
			return NULL;
		}
#endif

	if((result > 0) && FD_ISSET(fd, &fds)) {
		/* reaching here means serial port has data to read */
		num_bytes_to_read = count; /* initial value */
		do {
			if(partial_data == 1) {
				num_bytes_to_read = count - total_read;
			}

			errno = 0;
			ret = read(fd, buffer, num_bytes_to_read);

			if((ret > 0) && (errno == 0)) {
				total_read = total_read + ret;
				/* This indicates we got success and have read data. */
				/* If there is partial data read previously, append this data. */
				if(partial_data == 1) {
					for(i = 0; i < ret; i++) {
						final_buf[index] = buffer[i];
						index++;
					}
					/* Pass the final fully successful read to java layer. */
					dataRead = (*env)->NewByteArray(env, index);
					if((dataRead == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
						throw_serialcom_exception(env, 3, 0, E_NEWBYTEARRAYSTR);
						return NULL;
					}
					(*env)->SetByteArrayRegion(env, dataRead, 0, index, final_buf);
					if((*env)->ExceptionOccurred(env)) {
						throw_serialcom_exception(env, 3, 0, E_SETBYTEARRREGIONSTR);
						return NULL;
					}
					return dataRead;
				}else {
					/* Pass the successful read to java layer straight away. */
					dataRead = (*env)->NewByteArray(env, ret);
					if((dataRead == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
						throw_serialcom_exception(env, 3, 0, E_NEWBYTEARRAYSTR);
						return NULL;
					}
					(*env)->SetByteArrayRegion(env, dataRead, 0, ret, buffer);
					if((*env)->ExceptionOccurred(env)) {
						throw_serialcom_exception(env, 3, 0, E_SETBYTEARRREGIONSTR);
						return NULL;
					}
					return dataRead;
				}
			}else if((ret > 0) && (errno == EINTR)) {
				total_read = total_read + ret;
				/* This indicates, there is data to read, however, we got interrupted before we finish reading
				 * all of the available data. So we need to save this partial data and get back to read remaining. */
				for(i = 0; i < ret; i++) {
					final_buf[index] = buffer[i];
					index++;
				}
				partial_data = 1;
				errno = 0;
				continue;
			}else if(ret < 0) {
				if(errno == EINTR) {
					/* This indicates that we should retry as we are just interrupted by a signal. */
					errno = 0;
					continue;
				}else {
					/* This indicates, irrespective of, there was data to read or not, we got an error
					 * during operation. */
					throw_serialcom_exception(env, 1, errno, NULL);
					return NULL;
				}
			}else if(ret == 0) {
				/* This indicates, no data on port, EOF or port is removed from system (may be). */
				return NULL;
			}else {
				/* do nothing, relax :) */
			}
		} while(1);
	}

	/* should not be reached */
	return NULL;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    readBytesDirect
 * Signature: (JLjava/nio/ByteBuffer;II)I
 *
 * It does not modify the direct byte buffer attributes position, capacity, limit and mark. The
 * application design is expected to take care of this as and when required in appropriate manner.
 *
 * @return number of bytes read from serial port, 0 if there was no data in serial port buffer, -1
 *        if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_readBytesDirect(JNIEnv *env,
		jobject obj, jlong fd, jobject buffer, jint offset, jint length) {

	int ret = 0;
	int num_bytes_to_read = 0;
	int num_bytes_read_from_port = 0;
	int index = 0;
	jbyte* data_buf = NULL;

	int i = 0;
	int iovcount = 0;
	struct iovec* vec = NULL;
	struct iovec* vec_next = NULL;
	int have_last_vec_length = 0;
	int last_vector_length = 0;

	/* get base address of this buffer */
	data_buf = (jbyte *) (*env)->GetDirectBufferAddress(env, buffer);
	if((data_buf == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_GETDIRCTBUFADDRSTR);
		return -1;
	}

	if(length <= 3072) {
		/* non-vectored read() operation is required */
		index = offset;
		num_bytes_to_read = length;
		while(1) {
			errno = 0;
			ret = read(fd, &data_buf[index], num_bytes_to_read);
			if((ret > 0) && (errno == 0)) {
				/* This indicates we got success and have read data completely. */
				num_bytes_read_from_port = num_bytes_read_from_port + ret;
				return num_bytes_read_from_port;
			}else if((ret > 0) && (errno == EINTR)) {
				/* This indicates, there is data to read, however, we got interrupted before
				 * we finish reading all of the available data. Partial read scenario. */
				index = index + ret;
				num_bytes_to_read = num_bytes_to_read - ret;
				num_bytes_read_from_port = num_bytes_read_from_port + ret;
				errno = 0;
				continue;
			}else if(ret == 0) {
				/* this indicate there is no data in serial port buffer */
				return 0;
			}else if(ret < 0) {
				if(errno == EINTR) {
					/* This indicates that we should retry as we are just interrupted by a signal. */
					errno = 0;
					continue;
				}else {
					/* This indicates, irrespective of, there was data to read or not, we got an error
					 * during operation. */
					throw_serialcom_exception(env, 1, errno, NULL);
					return -1;
				}
			}else {
			}
		}
	}

	/* reaching here means, vectored I/O read() operation is required */
	iovcount = length / 3072;
	last_vector_length = length % 3072;
	if(last_vector_length > 0) {
		iovcount = iovcount + 1;
		have_last_vec_length = 1;
	}
	if(iovcount > 500) {
		/* for insane values of length throw error */
		throw_serialcom_exception(env, 3, 0, E_VIOVNTINVALIDSTR);
		return -1;
	}

	vec = (struct iovec*) calloc(iovcount, sizeof(struct iovec));
	if(vec == NULL) {
		throw_serialcom_exception(env, 3, 0, E_CALLOCSTR);
		return -1;
	}

	index = offset;
	vec_next = vec;
	for(i=0; i < iovcount; i++) {
		vec_next->iov_base = &data_buf[index];
		if(i != (iovcount - 1)) {
			/* length of all blocks before last will be 3072 */
			vec_next->iov_len = 3072;
		}else {
			/* length of last block may be 3072 or less than 3072 */
			if(have_last_vec_length == 1) {
				vec_next->iov_len = last_vector_length;
			}else {
				vec_next->iov_len = 3072;
			}
		}
		++vec_next;
		index = index + 3072;
	}

	index = offset;
	while(1) {
		errno = 0;
		ret = readv(fd, vec, iovcount);
		if((ret > 0) && (errno == 0)) {
			/* This indicates we got success and have read data completely. */
			free(vec);
			return ret;
		}else if((ret > 0) && (errno == EINTR)) {
			/* This indicates, there is data to read, however, we got interrupted before
			 * we finish reading all of the available data. Partial read scenario. */
			index = index + ret;
			num_bytes_to_read = num_bytes_to_read - ret;
			num_bytes_read_from_port = num_bytes_read_from_port + ret;
			while(1) {
				errno = 0;
				ret = read(fd, &data_buf[index], num_bytes_to_read);
				if((ret > 0) && (errno == 0)) {
					/* This indicates we got success and have read data completely. */
					free(vec);
					num_bytes_read_from_port = num_bytes_read_from_port + ret;
					return num_bytes_read_from_port;
				}else if((ret > 0) && (errno == EINTR)) {
					/* This indicates, there is data to read, however, we got interrupted before
					 * we finish reading all of the available data. Partial read scenario. */
					index = index + ret;
					num_bytes_to_read = num_bytes_to_read - ret;
					num_bytes_read_from_port = num_bytes_read_from_port + ret;
					errno = 0;
					continue;
				}else if(ret == 0) {
					/* this indicate there is no data in serial port buffer */
					free(vec);
					return 0;
				}else if(ret < 0) {
					if(errno == EINTR) {
						/* This indicates that we should retry as we are just interrupted by a signal. */
						errno = 0;
						continue;
					}else {
						/* This indicates, irrespective of, there was data to read or not, we got an error
						 * during operation. */
						free(vec);
						throw_serialcom_exception(env, 1, errno, NULL);
						return -1;
					}
				}else {
				}
			}
		}else if(ret == 0) {
			/* this indicate there is no data in serial port buffer */
			free(vec);
			return 0;
		}else if(ret < 0) {
			if(errno == EINTR) {
				/* This indicates that we should retry as we are just interrupted by a signal
				 * and have not read any data actually. */
				errno = 0;
				continue;
			}else {
				/* This indicates, irrespective of, there was data to read or not, we got an error
				 * during operation. */
				free(vec);
				throw_serialcom_exception(env, 1, errno, NULL);
				return -1;
			}
		}else {
		}
	}

	free(vec);
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    writeSingleByte
 * Signature: (JB)I
 *
 * Writes single data byte to serial port to be sent to target end.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_writeSingleByte(JNIEnv *env,
		jobject obj, jlong fd, jbyte dataByte) {

	int ret = -1;
	while(1) {
		errno = 0;
		ret = write(fd, &dataByte, 1);
		if(ret > 0) {
			return 0;
		}else if(ret < 0) {
			if(errno == EINTR) {
				errno = 0; // reset.
				continue;
			}else {
				throw_serialcom_exception(env, 1, errno, NULL);
				return -1;
			}
		}else {
			/* should not happen. */
			continue;
		}
	}
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    writeBytes
 * Signature: (J[BI)I
 *
 * Try writing all data using a loop by handling partial writes. tcdrain() waits until all output written
 * to the object referred to by fd has been transmitted. This is used to make sure that data gets sent out
 * of the serial port physically before write returns.
 *
 * If the number of bytes to be written is 0, then behavior is undefined as per POSIX standard. Therefore
 * we do not allow dummy writes with absolutely no data at all and this is handled at java layer. This
 * function does not block any signals.
 *
 * To segregate issues with buffer size or handling from device or driver specific implementation consider
 * using pseudo terminals (/dev/pts/1). If this works then check termios structure settings for real device.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_writeBytes(JNIEnv *env,
		jobject obj, jlong fd, jbyteArray buffer, jint delay) {

	int ret = -1;
	int index = 0;
	int status = 0;
	jbyte* data_buf = NULL;

	/* The JVM may return pointer to original buffer or pointer to its copy depending upon
	 * whether underlying garbage collector supports pinning or not. */
	data_buf = (*env)->GetByteArrayElements(env, buffer, JNI_FALSE);
	if((data_buf == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_GETBYTEARRELEMTSTR);
		return -1;
	}
	size_t count = (size_t) (*env)->GetArrayLength(env, buffer);

	/* Java layer checked that buffer must contain at least one byte to write. However if
	 * somebody extends class and by pass this check than we still prevent 0 byte write call. */
	if(count == 0) {
		throw_serialcom_exception(env, 3, 0, E_WRITEZERONOTALLOWED);
		return -1;
	}

	if(delay == 0) {
		while(count > 0) {
			errno = 0;
			ret = write(fd, &data_buf[index], count);
			if(ret < 0) {
				if(errno == EINTR) {
					serial_delay(20); /* 20 milliseconds delay just to let the cause of signal go away */
					errno = 0;
					continue;
				}else {
					status = (-1 * errno);
					break;
				}
			}else if(ret == 0) {
				errno = 0;
				continue;
			}else {
			}

			count = count - ret;
			index = index + ret;
		}
		tcdrain(fd);
	}else {
		while(count > 0) {
			errno = 0;
			ret = write(fd, &data_buf[index], 1);
			if(ret < 0) {
				if(errno == EINTR) {
					serial_delay(delay);
					errno = 0;
					continue;
				}else {
					status = (-1 * errno);
					break;
				}
			}else if(ret == 0) {
				errno = 0;
				continue;
			}else {
			}

			count = count - ret;
			index = index + ret;
			if(count != 0) {
				tcdrain(fd);         /* flush single byte out of serial port physically */
				serial_delay(delay); /* use supplied delay between bytes in milliseconds */
			}
		}
	}

	(*env)->ReleaseByteArrayElements(env, buffer, data_buf, 0);
	if(status < 0) {
		throw_serialcom_exception(env, 1, (-1 * status), NULL);
		return -1;
	}
	return status;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    writeBytesDirect
 * Signature: (JLjava/nio/ByteBuffer;II)I
 *
 * Sends data bytes from Java NIO direct byte buffer out of serial port from the given position upto
 * length number of bytes. If the number of bytes to be written is less than or equal to 3*1024
 * non-vectored write() is used otherwise vectored writev() is used.
 *
 * This function handles partial write scenario for both vectored and non-vectored write operations.
 *
 * It does not modify the direct byte buffer attributes position, capacity, limit and mark. The application
 * design is expected to take care of this as and when required in appropriate manner in Java layer itself.
 * Also it does not consume or modify the data in the given buffer.
 *
 * @return number of bytes written to serial port on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_writeBytesDirect(JNIEnv *env,
		jobject obj, jlong fd, jobject buffer, jint offset, jint length) {

	jbyte* data_buf = NULL;
	int ret = 0;
	int count = 0;
	int index = 0;
	int num_bytes_written = 0;

	int i = 0;
	int iovcount = 0;
	struct iovec* vec = NULL;
	struct iovec* vec_next = NULL;
	int have_last_vec_length = 0;
	int last_vector_length = 0;
	int new_length = 0;

	/* get base address of this buffer */
	data_buf = (jbyte *) (*env)->GetDirectBufferAddress(env, buffer);
	if((data_buf == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_GETDIRCTBUFADDRSTR);
		return -1;
	}

	if(length <= 3072) {
		/* non-vectored write() operation is required */
		index = offset;
		count = length;
		while(count > 0) {
			errno = 0;
			ret = write(fd, &data_buf[index], (size_t)count);
			if(ret < 0) {
				if(errno == EINTR) {
					errno = 0;
					continue;
				}else {
					throw_serialcom_exception(env, 1, errno, NULL);
					return -1;
				}
			}else if(ret == 0) {
				errno = 0;
				continue;
			}else {
			}
			num_bytes_written = num_bytes_written + ret;
			count = count - ret;
			index = index + ret;
			tcdrain(fd);
		}
		return num_bytes_written;
	}

	/* reaching here means, vectored I/O write() operation is required */
	iovcount = length / 3072;
	last_vector_length = length % 3072;
	if(last_vector_length > 0) {
		iovcount = iovcount + 1;
		have_last_vec_length = 1;
	}
	if(iovcount > 500) {
		/* for insane values of length throw error */
		throw_serialcom_exception(env, 3, 0, E_VIOVNTINVALIDSTR);
		return -1;
	}

	vec = (struct iovec*) calloc(iovcount, sizeof(struct iovec));
	if(vec == NULL) {
		throw_serialcom_exception(env, 3, 0, E_CALLOCSTR);
		return -1;
	}

	index = offset;
	vec_next = vec;
	for(i=0; i < iovcount; i++) {
		vec_next->iov_base = &data_buf[index];
		if(i != (iovcount - 1)) {
			/* length of all blocks before last will be 3072 */
			vec_next->iov_len = 3072;
		}else {
			/* length of last block may be 3072 or less than 3072 */
			if(have_last_vec_length == 1) {
				vec_next->iov_len = last_vector_length;
			}else {
				vec_next->iov_len = 3072;
			}
		}
		++vec_next;
		index = index + 3072;
	}

	index = offset;
	count = length;
	while(count > 0) {
		errno = 0;
		ret = writev(fd, vec, iovcount);
		if(ret < 0) {
			if(errno == EINTR) {
				errno = 0;
				continue;
			}else {
				free(vec);
				throw_serialcom_exception(env, 1, errno, NULL);
				return -1;
			}
		}else if(ret == 0) {
			errno = 0;
			continue;
		}else {
		}
		num_bytes_written = num_bytes_written + ret;
		count = count - ret;
		index = index + ret;
		tcdrain(fd);

		if(count > 0 ) {
			/* reaching here means need to handle partial write scenario */
			new_length = length - num_bytes_written;
			iovcount = new_length / 3072;

			if(iovcount == 0) {
				/* reaching here means, number of bytes after partial write is less than 3072,
				 * so use non-vectored write system call. */
				while(count > 0) {
					errno = 0;
					ret = write(fd, &data_buf[index], (size_t)count);
					if(ret < 0) {
						if(errno == EINTR) {
							errno = 0;
							continue;
						}else {
							free(vec);
							throw_serialcom_exception(env, 1, errno, NULL);
							return -1;
						}
					}else if(ret == 0) {
						errno = 0;
						continue;
					}else {
					}
					num_bytes_written = num_bytes_written + ret;
					count = count - ret;
					index = index + ret;
					tcdrain(fd);
				}
				free(vec);
				return num_bytes_written;
			}

			/* reaching here means vector IO operation is still required. */
			have_last_vec_length = 0;
			last_vector_length = new_length % 3072;
			if(last_vector_length > 0) {
				iovcount = iovcount + 1;
				have_last_vec_length = 1;
			}

			free(vec);
			vec = (struct iovec*) calloc(iovcount, sizeof(struct iovec));
			if(vec == NULL) {
				throw_serialcom_exception(env, 3, 0, E_CALLOCSTR);
				return -1;
			}

			vec_next = vec;
			for(i=0; i < iovcount; i++) {
				vec_next->iov_base = &data_buf[index];
				if(i != (iovcount - 1)) {
					/* length of all blocks before last will be 3072 */
					vec_next->iov_len = 3072;
				}else {
					/* length of last block may be 3072 or less than 3072 */
					if(have_last_vec_length == 1) {
						vec_next->iov_len = last_vector_length;
					}else {
						vec_next->iov_len = 3072;
					}
				}
				++vec_next;
				index = index + 3072;
			}
		}
	}

	free(vec);
	return num_bytes_written;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    createBlockingIOContext
 * Signature: ()J
 *
 * This will create event object/file descriptor that will be used to wait upon in addition to
 * serial port file descriptor, so as to bring blocked read call out of waiting state. This is needed
 * if application is willing to close the serial port but unable because a blocked reader exist.
 *
 * @return context on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jlong JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_createBlockingIOContext(JNIEnv *env,
		jobject obj) {

#if defined (__linux__)
	int evfd = 0;
	errno = 0;
	evfd  = eventfd(0, 0);
	if(evfd < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
	return evfd;
#endif

#if defined (__APPLE__)
	int ret = -1;
	jlong *pipeinfo = NULL;
	int pipefdpair[2];
	errno = 0;
	ret = pipe(pipefdpair);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
	pipeinfo = (jlong *) calloc(2, sizeof(jlong));
	if(pipeinfo == NULL) {
		throw_serialcom_exception(env, 3, 0, E_CALLOCSTR);
		return -1;
	}
	/* pipe1[0] is reading end, and pipe1[1] is writing end. */
	pipeinfo[0] = pipefdpair[0];
	pipeinfo[1] = pipefdpair[1];
	return pipeinfo;
#endif

	/* should not be reached */
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    unblockBlockingIOOperation
 * Signature: (J)I
 *
 * Causes data event or event as required to emulate an event.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_unblockBlockingIOOperation(JNIEnv *env,
		jobject obj, jlong context) {

#if defined (__linux__)
	int ret;
	uint64_t value = 5;
	errno = 0;
	ret = write(((int) context), &value, sizeof(value));
	if(ret <= 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#endif

#if defined (__APPLE__)
	int ret;
	ret = write(((int) context[1]), "EXIT", strlen("EXIT"));
	if(ret <= 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#endif

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    destroyBlockingIOContext
 * Signature: (J)I
 *
 * Closes the event file descriptors and releases memory if it was allocated.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_destroyBlockingIOContext(JNIEnv *env,
		jobject obj, jlong context) {

#if defined (__linux__)
	close(context);
#endif

#if defined (__APPLE__)
	close((int) context[0]);
	close((int) context[1]);
	free(context);
#endif

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    configureComPortData
 * Signature: (JIIIII)I
 *
 * Configures format of data that will be exchanged through serial port electrically.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_configureComPortData(JNIEnv *env,
		jobject obj, jlong fd, jint dataBits, jint stopBits, jint parity, jint baudRateTranslated,
		jint custBaudTranslated) {

	int ret = 0;

#if defined (__linux__)
	struct termios2 currentconfig = {0};
	errno = 0;
	ret = ioctl(fd, TCGETS2, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#elif defined (__APPLE__) || defined (__SunOS)
	struct termios currentconfig = {0};
	errno = 0;
	ret = tcgetattr(fd, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#else
#endif

	/* We handle custom baud rate case first so as to make development/DBGging easy for developers. */
	if(baudRateTranslated == 251) {
#if defined (__linux__)
		currentconfig.c_cflag &= ~CBAUD;
		currentconfig.c_cflag |= BOTHER;
		currentconfig.c_ispeed = custBaudTranslated;
		currentconfig.c_ospeed = custBaudTranslated;

		errno = 0;
		ret = ioctl(fd, TCSETS2, &currentconfig);
		if(ret < 0) {
			throw_serialcom_exception(env, 1, errno, NULL);
			return -1;
		}

#elif defined (__APPLE__)
		speed_t speed = custBaudTranslated;
		errno = 0;
		ret = ioctl(fd, IOSSIOSPEED, &speed);
		if(ret < 0) {
			throw_serialcom_exception(env, 1, errno, NULL);
			return -1;
		}

#elif defined (__SunOS)
		/* Solaris does not support custom baud rates. */
		LOGE("This baud rate is not supported by solaris.");
#else
#endif

	}else {
		/* Handle standard baud rate setting. */
		int baud = -1;
		/* Baudrate support depends upon operating system, driver and chipset used. */
		switch (baudRateTranslated) {
		case 0: baud = B0;
		break;
		case 50: baud = B50;
		break;
		case 75: baud = B75;
		break;
		case 110: baud = B110;
		break;
		case 134: baud = B134;
		break;
		case 150: baud = B150;
		break;
		case 200: baud = B200;
		break;
		case 300: baud = B300;
		break;
		case 600: baud = B600;
		break;
		case 1200: baud = B1200;
		break;
		case 1800: baud = B1800;
		break;
		case 2400: baud = B2400;
		break;
		case 4800: baud = B4800;
		break;
		case 9600: baud = B9600;
		break;
		case 14400: baud = 14400;
		break;
		case 19200: baud = B19200;
		break;
		case 28800: baud = 28800;
		break;
		case 38400: baud = B38400;
		break;
		case 56000: baud = 56000;
		break;
		case 57600: baud = B57600;
		break;
		case 115200: baud = B115200;
		break;
		case 128000: baud = 128000;
		break;
		case 153600: baud = 153600;
		break;
		case 230400: baud = B230400;
		break;
		case 256000: baud = 256000;
		break;
		case 460800: baud = 460800;
		break;
		case 500000: baud = 500000;
		break;
		case 576000: baud = 576000;
		break;
		case 921600: baud = 921600;
		break;
		case 1000000: baud = 1000000;
		break;
		case 1152000: baud = 1152000;
		break;
		case 1500000: baud = 1500000;
		break;
		case 2000000: baud = 2000000;
		break;
		case 2500000: baud = 2500000;
		break;
		case 3000000: baud = 3000000;
		break;
		case 3500000: baud = 3500000;
		break;
		case 4000000: baud = 4000000;
		break;
		default: baud = -1;
		break;
		}
#if defined (__linux__)
					currentconfig.c_ispeed = baud;
currentconfig.c_ospeed = baud;
#elif defined (__APPLE__) || defined (__SunOS)
errno = 0;
ret = cfsetspeed(&currentconfig, baud);
if(ret < 0) {
	throw_serialcom_exception(env, 1, errno, NULL);
	return -1;
}
#else
#endif
	}

	/* Set data bits. */
	currentconfig.c_cflag &= ~CSIZE;
	switch(dataBits) {
	case 5:
		currentconfig.c_cflag |= CS5;
		break;
	case 6:
		currentconfig.c_cflag |= CS6;
		break;
	case 7:
		currentconfig.c_cflag |= CS7;
		break;
	case 8:
		currentconfig.c_cflag |= CS8;
		break;
	}

	/* Set stop bits. If CSTOPB is not set one stop bit is used. Otherwise two stop bits are used. */
	if(stopBits == 1) {
		currentconfig.c_cflag &= ~CSTOPB; /* one stop bit used if user set 1 stop bit */
	}else {
		currentconfig.c_cflag |= CSTOPB;  /* two stop bits used if user set 1.5 or 2 stop bits */
	}

	/* Clear existing parity and then set new parity.
	 * INPCK  : Enable checking parity of data.
	 * ISTRIP : Strip parity bit from data before sending it to application.
	 * CMSPAR : Mark or space (stick) parity (Linux OS). Not is POSIX, requires _BSD_SOURCE.
	 * PAREXT : Extended parity for mark and space parity (AIX OS).  */
#if defined(CMSPAR)
	currentconfig.c_cflag &= ~(PARENB | PARODD | CMSPAR);
#elif defined(PAREXT)
	currentconfig.c_cflag &= ~(PARENB | PARODD | PAREXT);
#else
	currentconfig.c_cflag &= ~(PARENB | PARODD);
#endif

	switch(parity) {
	case 1:
		currentconfig.c_cflag &= ~PARENB;                    /* No parity */
		break;
	case 2:
		currentconfig.c_cflag |= (PARENB | PARODD);          /* Odd parity */
		currentconfig.c_iflag |= (INPCK);
		break;
	case 3:
		currentconfig.c_cflag |= PARENB;                     /* Even parity */
		currentconfig.c_cflag &= ~PARODD;
		currentconfig.c_iflag |= (INPCK);
		break;
	case 4:
#if defined(CMSPAR)
		currentconfig.c_cflag |= (PARENB | PARODD | CMSPAR); /* Mark parity */
#elif defined(PAREXT)
		currentconfig.c_cflag |= (PARENB | PARODD | PAREXT);
#endif
		currentconfig.c_iflag |= (INPCK);
		break;
	case 5:
#if defined(CMSPAR)
		currentconfig.c_cflag |= (PARENB | CMSPAR);          /* Space parity */
#elif defined(PAREXT)
		currentconfig.c_cflag |= (PARENB | PAREXT);
#endif
		currentconfig.c_cflag &= ~PARODD;
		currentconfig.c_iflag |= (INPCK);
		break;
	}

	/* Apply changes/settings to the termios associated with this port. */
#if defined (__linux__)
	ioctl(fd, TCSETS2, &currentconfig);

#elif defined (__APPLE__) || defined (__SunOS)
	errno = 0;
	ret  = tcsetattr(fd, TCSANOW, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#else
#endif

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    configureComPortControl
 * Signature: (JIBBZZ)I
 *
 * Defines how the data communication through serial port will be controlled.
 *
 * For software flow control; IXON, IXOFF, and IXANY are used . If IXOFF is set, then software
 * flow control is enabled on the TTY's input queue. The TTY transmits a STOP character when the
 * program cannot keep up with its input queue and transmits a START character when its input queue
 * in nearly empty again. If IXON is set, software flow control is enabled on the TTY's output queue.
 * The TTY blocks writes by the program when the device to which it is connected cannot keep up with
 * it. If IXANY is set, then any character received by the TTY from the device restarts the output
 * that has been suspended.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_configureComPortControl(JNIEnv *env,
		jobject obj, jlong fd, jint flowctrl, jbyte xon, jbyte xoff,
		jboolean ParFraError, jboolean overFlowErr) {

	int ret = 0;

#if defined (__linux__)
	struct termios2 currentconfig = {0};
	errno = 0;
	ret = ioctl(fd, TCGETS2, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#elif defined (__APPLE__) || defined (__SunOS)
	struct termios currentconfig = {0};
	errno = 0;
	ret = tcgetattr(fd, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#else
#endif

	/* Output options :
	 * raw output, no processing at all. */
	currentconfig.c_oflag = 0;

	/* Line options :
	 * Non-canonical mode is enabled, echo nothing, deliver no signals. */
	currentconfig.c_lflag = 0;

#if defined (__linux__)
	/* Line discipline */
	currentconfig.c_line = 0;
#endif

	/* Control options :
	 * CREAD and CLOCAL are enabled to make sure that the caller program does not become the owner of the
	 * port subject to sporadic job control and hang-up signals, and also that the serial interface driver
	 * will read incoming bytes. CLOCAL results in ignoring modem status lines while CREAD enables receiving
	 * data.Note that CLOCAL need always be set to prevent undesired effects of SIGNUP SIGNAL. */
	currentconfig.c_cflag |= (CREAD | CLOCAL | HUPCL);

	/* Input options : */
	currentconfig.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | IXANY | IXON | IXOFF | INPCK | ISTRIP | BRKINT);
#ifdef IUCLC
	currentconfig.c_iflag &= ~IUCLC;  /* translate upper case to lower case */
#endif

	/* Blocking read with 100ms timeout (VTIME *0.1 seconds). If caller requested say 20 bytes and they are available
	 * in OS buffer, then it's returned to the caller immediately and without having VMIN and VTIME participate in any way. */
	currentconfig.c_cc[VTIME] = 1;
	currentconfig.c_cc[VMIN] = 0;

	/* Set flow control. The CRTSCTS for Solaris enables outbound hardware flow control if set, while for Linux and Mac enables both inbound and outbound. */
	if(flowctrl == 1) {                                    /* NO FLOW CONTROL. */
		currentconfig.c_iflag &= ~(IXON | IXOFF | IXANY);
#if defined (__linux__)
		currentconfig.c_cflag &= ~CRTSCTS;
#endif
#if defined (__APPLE__)
		currentconfig.c_cflag &= ~CRTSCTS;
		currentconfig.c_cflag &= ~CRTS_IFLOW;
		currentconfig.c_cflag &= ~CCTS_OFLOW;
#endif
#if defined (__SunOS)
		currentconfig.c_cflag &= ~CRTSXOFF;
		currentconfig.c_cflag &= ~CRTSCTS;
#endif
	}else if(flowctrl == 2) {                              /* HARDWARE FLOW CONTROL on both tx and rx data. */
		currentconfig.c_iflag &= ~(IXON | IXOFF);           /* software xon-xoff character disabled. */
#if defined (__linux__)
		currentconfig.c_cflag |= CRTSCTS;                   /* Specifying hardware flow control. */
#endif
#if defined (__APPLE__)
		currentconfig.c_cflag |= CRTSCTS;
		currentconfig.c_cflag |= CRTS_IFLOW;
		currentconfig.c_cflag |= CCTS_OFLOW;
#endif
#if defined (__SunOS)
		currentconfig.c_cflag |= CRTSXOFF;
		currentconfig.c_cflag |= CRTSCTS;
#endif
	}else if(flowctrl == 3) {                              /* SOFTWARE FLOW CONTROL on both tx and rx data. */
		currentconfig.c_cflag &= ~CRTSCTS;                  /* hardware rts-cts disabled. */
		currentconfig.c_iflag |= (IXON | IXOFF);            /* software xon-xoff chararcter enabled. */
		currentconfig.c_cc[VSTART] = (unsigned char) xon; /* The value of the XON character for both transmission and reception. */
		currentconfig.c_cc[VSTOP] = (unsigned char) xoff; /* The value of the XOFF character for both transmission and reception. */
	}else {
	}

	/* Set parity and frame error. */
	if(ParFraError == JNI_TRUE) {
		/* First check if user has enabled parity checking or not. */
		if(!((currentconfig.c_cflag & PARENB) == PARENB)) {
			throw_serialcom_exception(env, 3, 0, E_ENBLPARCHKSTR);
			return -1;
		}

		/* Mark the character as containing an error. This will cause a character containing a parity or framing error to be
		 * replaced by a three character sequence consisting of the erroneous character preceded by \377 and \000. A legitimate
		 * received \377 will be replaced by a pair of \377s.*/
		currentconfig.c_iflag &= ~IGNPAR;
		currentconfig.c_iflag |=  PARMRK;
	}else {
		/* Ignore the character containing an error. Any received characters containing parity errors will be silently dropped. */
		currentconfig.c_iflag |=  IGNPAR;
		currentconfig.c_iflag |=  PARMRK;

		currentconfig.c_iflag &= ~IGNPAR;
		currentconfig.c_iflag &= ~PARMRK;
	}

	/* Set buffer overrun error.
	 * Echo the ASCII BEL character, 0x07 or as defined in 'c_cc' when the input stream overflows.
	 * Additional data is lost.  If MAXBEL is not set, the BEL character is not sent but the data is lost anyhow. */
	if(overFlowErr == JNI_TRUE) {
		currentconfig.c_iflag |= IMAXBEL;
	}else {
		currentconfig.c_iflag &= ~IMAXBEL;
	}

	/* Apply changes/settings to the termios associated with this port. */
#if defined (__linux__)
	errno = 0;
	ret = ioctl(fd, TCSETS2, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

#elif defined (__APPLE__) || defined (__SunOS)
	errno = 0;
	ret  = tcsetattr(fd, TCSANOW, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#else
#endif

	/* Clear IO buffers after applying new valid settings to make port in 100% sane condition. */
#if defined (__linux__)
	ioctl(fd, TCFLSH, TCIOFLUSH);
#elif defined (__APPLE__) || defined (__SunOS)
	tcflush(fd, TCIOFLUSH);
#else
#endif

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getCurrentConfigurationU
 * Signature: (J)[I
 *
 * We return the bit mask as it is with out interpretation so that application can manipulate easily
 * using mathematics.
 *
 * @return serial port configuration array constructed out of termios structure on success otherwise
 *         NULL if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jintArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getCurrentConfigurationU(JNIEnv *env,
		jobject obj, jlong fd) {

	int ret = -1;
	jint err[] = {-1};
	jintArray errr = NULL;
	jintArray configuration = NULL;
#if defined (__linux__)
	jint settings[25];
	struct termios2 currentconfig = {0};
#elif defined (__APPLE__) || defined (__SunOS)
	jint settings[23];
	struct termios currentconfig = {0};
#else
#endif

#if defined (__linux__)
	configuration = (*env)->NewIntArray(env, 25);
	errno = 0;
	ret = ioctl(fd, TCGETS2, &currentconfig);
	if(ret < 0) {
		errr = (*env)->NewIntArray(env, 1);
		(*env)->SetIntArrayRegion(env, errr, 0, 1, err);
		return errr;
	}

#elif defined (__APPLE__) || defined (__SunOS)
	configuration = (*env)->NewIntArray(env, 23);
	errno = 0;
	ret = tcgetattr(fd, &currentconfig);
	if(ret < 0) {
		errr = (*env)->NewIntArray(env, 1);
		(*env)->SetIntArrayRegion(env, errr, 0, 1, err);
		return errr;
	}
#else
#endif

	/* Populate array with current settings. */
#if defined (__linux__)
	settings[0] = 0;
	settings[1] = (jint) currentconfig.c_iflag;
	settings[2] = (jint) currentconfig.c_oflag;
	settings[3] = (jint) currentconfig.c_cflag;
	settings[4] = (jint) currentconfig.c_lflag;
	settings[5] = (jint) currentconfig.c_line;
	settings[6] = (jint) currentconfig.c_cc[0];
	settings[7] = (jint) currentconfig.c_cc[1];
	settings[8] = (jint) currentconfig.c_cc[2];
	settings[9] = (jint) currentconfig.c_cc[3];
	settings[10] = (jint) currentconfig.c_cc[4];
	settings[11] = (jint) currentconfig.c_cc[5];
	settings[12] = (jint) currentconfig.c_cc[6];
	settings[13] = (jint) currentconfig.c_cc[7];
	settings[14] = (jint) currentconfig.c_cc[8];
	settings[15] = (jint) currentconfig.c_cc[9];
	settings[16] = (jint) currentconfig.c_cc[10];
	settings[17] = (jint) currentconfig.c_cc[11];
	settings[18] = (jint) currentconfig.c_cc[12];
	settings[19] = (jint) currentconfig.c_cc[13];
	settings[20] = (jint) currentconfig.c_cc[14];
	settings[21] = (jint) currentconfig.c_cc[15];
	settings[22] = (jint) currentconfig.c_cc[16];
	settings[23] = (jint) currentconfig.c_ispeed;
	settings[24] = (jint) currentconfig.c_ospeed;

	(*env)->SetIntArrayRegion(env, configuration, 0, 25, settings);

#elif defined (__APPLE__) || defined (__SunOS)
	settings[0] = 0;
	settings[1] = (jint) currentconfig.c_iflag;
	settings[2] = (jint) currentconfig.c_oflag;
	settings[3] = (jint) currentconfig.c_cflag;
	settings[4] = (jint) currentconfig.c_lflag;
	settings[5] = (jint) currentconfig.c_cc[0];
	settings[6] = (jint) currentconfig.c_cc[1];
	settings[7] = (jint) currentconfig.c_cc[2];
	settings[8] = (jint) currentconfig.c_cc[3];
	settings[9] = (jint) currentconfig.c_cc[4];
	settings[10] = (jint) currentconfig.c_cc[5];
	settings[11] = (jint) currentconfig.c_cc[6];
	settings[12] = (jint) currentconfig.c_cc[7];
	settings[13] = (jint) currentconfig.c_cc[8];
	settings[14] = (jint) currentconfig.c_cc[9];
	settings[15] = (jint) currentconfig.c_cc[10];
	settings[16] = (jint) currentconfig.c_cc[11];
	settings[17] = (jint) currentconfig.c_cc[12];
	settings[18] = (jint) currentconfig.c_cc[13];
	settings[19] = (jint) currentconfig.c_cc[14];
	settings[20] = (jint) currentconfig.c_cc[15];
	settings[21] = (jint) currentconfig.c_cc[16];

	(*env)->SetIntArrayRegion(env, configuration, 0, 22, settings);
#else
#endif

	return configuration;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getCurrentConfigurationW
 * Signature: (J)[Ljava/lang/String;
 *
 * Applicable for Windows OS only.
 *
 * @return NULL always.
 */
JNIEXPORT jobjectArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getCurrentConfigurationW(JNIEnv *env,
		jobject obj, jlong handle) {
	return NULL;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getByteCount
 * Signature: (J)[I
 *
 * Return array's sequence is number of input bytes, number of output bytes in tty buffers.
 *
 * @return array containing number of bytes in input and output buffer on success otherwise
 *         NULL if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jintArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getByteCount(JNIEnv *env,
		jobject obj, jlong fd) {

	int ret = -1;
	jint val[2] = {0, 0};
	jintArray byteCounts = NULL;

	errno = 0;
	ret = ioctl(fd, FIONREAD, &val[0]);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return NULL;
	}

	errno = 0;
	ret = ioctl(fd, TIOCOUTQ, &val[1]);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return NULL;
	}

	byteCounts = (*env)->NewIntArray(env, 2);
	if((byteCounts == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_NEWINTARRAYSTR);
		return NULL;
	}

	(*env)->SetIntArrayRegion(env, byteCounts, 0, 2, val);
	if((*env)->ExceptionOccurred(env)) {
		throw_serialcom_exception(env, 3, 0, E_SETINTARRREGIONSTR);
		return NULL;
	}

	return byteCounts;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    clearPortIOBuffers
 * Signature: (JZZ)I
 *
 * This will discard all pending data in given buffers. Received data therefore can not be read by
 * application or/and data to be transmitted in output buffer will get discarded i.e. not transmitted.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_clearPortIOBuffers(JNIEnv *env,
		jobject obj, jlong fd, jboolean rxPortbuf, jboolean txPortbuf) {

	int ret = 0;
	int PORTIOBUFFER = 0;

	if((rxPortbuf == JNI_TRUE) && (txPortbuf == JNI_TRUE)) {
		/* flushes both the input and output queue. */
		PORTIOBUFFER = TCIOFLUSH;
	}else if(rxPortbuf == JNI_TRUE) {
		/* flushes the input queue, which contains data that have been received but not yet read. */
		PORTIOBUFFER = TCIFLUSH;
	}else if(txPortbuf == JNI_TRUE) {
		/* flushes the output queue, which contains data that have been written but not yet transmitted. */
		PORTIOBUFFER = TCOFLUSH;
	}else {
		/* this case is handled in java layer itself */
	}

	errno = 0;
	ret = tcflush(fd, PORTIOBUFFER);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    setRTS
 * Signature: (JZ)I
 *
 * Sets the RTS line to low or high voltages as defined by enabled argument. This causes value
 * in UART control register to change.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_setRTS(JNIEnv *env,
		jobject obj, jlong fd, jboolean enabled) {

	int ret = -1;
	int status = 0;

	/* Get current state details. */
	errno = 0;
	ret = ioctl(fd, TIOCMGET, &status);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	if(enabled == JNI_TRUE) {
		status |= TIOCM_RTS;
	}else {
		status &= ~TIOCM_RTS;
	}

	/* Update RTS line to desired state. */
	errno = 0;
	ret = ioctl(fd, TIOCMSET, &status);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    setDTR
 * Signature: (JZ)I
 *
 * Sets the DTR line to low or high voltages as defined by enabled argument. This causes value in
 * UART control register to change.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_setDTR(JNIEnv *env,
		jobject obj, jlong fd, jboolean enabled) {

	int ret = -1;
	int status = 0;

	errno = 0;
	ret = ioctl(fd, TIOCMGET, &status);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	if(enabled == JNI_TRUE) {
		status |= TIOCM_DTR;
	}else {
		status &= ~TIOCM_DTR;
	}

	errno = 0;
	ret = ioctl(fd, TIOCMSET, &status);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getLinesStatus
 * Signature: (J)[I
 *
 * The status of modem/control lines is returned as array of integers where '1' means line is asserted
 * and '0' means de-asserted. The sequence of lines matches in both java layer and native layer.
 *
 * Return sequence is CTS, DSR, DCD, RI, LOOP, RTS, DTR respectively.
 *
 * @return array containing status of modem control lines 0 on success otherwise NULL if
 *         an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jintArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getLinesStatus(JNIEnv *env,
		jobject obj, jlong fd) {

	int ret = -1;
	int lines_status = 0;
	jint status[7] = {0};
	jintArray current_status = NULL;

	current_status = (*env)->NewIntArray(env, 7);
	if((current_status == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_NEWINTARRAYSTR);
		return NULL;
	}

	errno = 0;
	ret = ioctl(fd, TIOCMGET, &lines_status);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return NULL;
	}

	status[0] = (lines_status & TIOCM_CTS) ? 1 : 0;
	status[1] = (lines_status & TIOCM_DSR) ? 1 : 0;
	status[2] = (lines_status & TIOCM_CD)  ? 1 : 0;
	status[3] = (lines_status & TIOCM_RI)  ? 1 : 0;
	status[4] = 0;
	status[5] = (lines_status & TIOCM_RTS) ? 1 : 0;
	status[6] = (lines_status & TIOCM_DTR) ? 1 : 0;

	(*env)->SetIntArrayRegion(env, current_status, 0, 7, status);
	if((*env)->ExceptionOccurred(env)) {
		throw_serialcom_exception(env, 3, 0, E_SETINTARRREGIONSTR);
		return NULL;
	}

	return current_status;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    findDriverServingComPort
 * Signature: (Ljava/lang/String;)Ljava/lang/String;
 *
 * Find the name of the driver which is currently associated with the given serial port.
 *
 * @return name of driver if found for given serial port, empty string if no driver found for
 *         given serial port, NULL if any error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jstring JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_findDriverServingComPort(JNIEnv *env,
		jobject obj, jstring comPortName) {
	return find_driver_for_given_com_port(env, comPortName);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    findIRQnumberForComPort
 * Signature: (J)Ljava/lang/String;
 *
 * Find the address and IRQ number associated with the given handle of serial port.
 *
 * @return address and IRQ string if found for given handle, empty string if no address/IRQ found for
 *         given handle, NULL if any error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jstring JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_findIRQnumberForComPort(JNIEnv *env,
		jobject obj, jlong fd) {
	return find_address_irq_for_given_com_port(env, fd);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    sendBreak
 * Signature: (JI)I
 *
 * The duration is in milliseconds. If the line is held in the logic low condition (space in
 * UART jargon) for longer than a character time, this is a break condition that can be detected by
 * the UART. This applies break condition as per EIA232 standard.
 *
 * Use this for testing rough timing fprintf(stderr, "%u\n", (unsigned)time(NULL));
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_sendBreak(JNIEnv *env,
		jobject obj, jlong fd, jint duration) {

	int ret = -1;

	/* Set break condition. */
	errno = 0;
	ret = ioctl(fd, TIOCSBRK, 0);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	ret = serial_delay(duration);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, (-1 * ret), NULL);
		return -1;
	}

	/* Release break condition. */
	errno = 0;
	ret = ioctl(fd, TIOCCBRK, 0);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getInterruptCount
 * Signature: (J)[I
 *
 * This is called when the user wants to know how many serial line interrupts have happened. If
 * the driver has an interrupt handler, it should define an internal structure of counters to keep
 * track of these statistics and increment the proper counter every time the function is run by the
 * kernel. This ioctl call passes the kernel a pointer to a structure serial_icounter_struct , which
 * should be filled by the tty driver.
 *
 * Not supported by Solaris and Mac OS itself (this function will return NULL).
 *
 * @return array containing interrupt count on success otherwise NULL if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jintArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getInterruptCount(JNIEnv *env,
		jobject obj, jlong fd) {

	jint count_info[11] = {0};
	jintArray interrupt_info = NULL;
#if defined(__linux__)
	int ret = -1;
	struct serial_icounter_struct counter = {0};
#endif

	interrupt_info = (*env)->NewIntArray(env, 11);
	if((interrupt_info == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
		throw_serialcom_exception(env, 3, 0, E_NEWINTARRAYSTR);
		return NULL;
	}

	errno = 0;
	ret = ioctl(fd , TIOCGICOUNT, &counter);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return NULL;
	}

	count_info[0] = counter.cts;
	count_info[1] = counter.dsr;
	count_info[2] = counter.rng;
	count_info[3] = counter.dcd;
	count_info[4] = counter.rx;
	count_info[5] = counter.tx;
	count_info[6] = counter.frame;
	count_info[7] = counter.overrun;
	count_info[8] = counter.parity;
	count_info[9] = counter.brk;
	count_info[10] = counter.buf_overrun;

	(*env)->SetIntArrayRegion(env, interrupt_info, 0, 11, count_info);
	return interrupt_info;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    fineTuneRead
 * Signature: (JIIIII)I
 *
 * This function gives more precise control over the behavior of read operation in terms of
 * timeout and number of bytes.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_fineTuneRead(JNIEnv *env,
		jobject obj, jlong fd, jint vmin, jint vtime, jint a, jint b, jint c) {

	int ret = -1;
#if defined (__linux__)
	struct termios2 currentconfig = {0};
#elif defined (__APPLE__) || defined (__SunOS)
	struct termios currentconfig = {0};
#else
#endif

#if defined (__linux__)
	errno = 0;
	ret = ioctl(fd, TCGETS2, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#elif defined (__APPLE__) || defined (__SunOS)
	errno = 0;
	ret = tcgetattr(fd, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#else
#endif

	currentconfig.c_cc[VMIN] = vmin;
	currentconfig.c_cc[VTIME] = vtime;

#if defined (__linux__)
	errno = 0;
	ret = ioctl(fd, TCSETS2, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#elif defined (__APPLE__) || defined (__SunOS)
	errno = 0;
	ret  = tcsetattr(fd, TCSANOW, &currentconfig);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#else
#endif

	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    setUpDataLooperThread
 * Signature: (JLcom/embeddedunveiled/serial/internal/SerialComLooper;)I
 *
 * Creates new native worker thread.
 *
 * Note that, GetMethodID() causes an uninitialized class to be initialized. However in our case
 * we have already initialized classes required.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_setUpDataLooperThread(JNIEnv *env,
		jobject obj, jlong fd, jobject looper) {

	int ret = -1;
	int x = -1;
	struct com_thread_params *ptr = NULL;
	jboolean entry_found = JNI_FALSE;
	jboolean empty_entry_found = JNI_FALSE;
	pthread_t thread_id = 0;
	jobject datalooper = NULL;
	struct com_thread_params params;
	void *arg;
	pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

	ptr = fd_looper_info;

	/* we make sure that thread creation, data passing and access to global data is atomic. */
	pthread_mutex_lock(&mutex);

	/* Check if there is an entry for this fd already in global array. If yes, we will update that
	 * with information about data thread. Further if there is an unused index we will re-use it. */
	for (x=0; x < MAX_NUM_THREADS; x++) {
		if((ptr->fd == fd) || (ptr->fd == -1)) {
			if(ptr->fd == fd) {
				entry_found = JNI_TRUE;
			}
			if(ptr->fd == -1) {
				empty_entry_found = JNI_TRUE;
			}
			break;
		}
		ptr++;
	}

	if((entry_found == JNI_TRUE) && (empty_entry_found == JNI_FALSE)) {
		/* Set up pointer to location which will be passed to thread (event thread probably
		 * exist for this fd so modify only data thread related arguments). */
		ptr->data_init_done = -1;
		ptr->data_standard_err_code = 0;
		ptr->data_custom_err_code = 0;
		ptr->data_cond_var = cond_var;
		arg = &fd_looper_info[x];
	}else if((entry_found == JNI_FALSE) && (empty_entry_found == JNI_TRUE)) {
		/* Set the values, create reference to it to be passed to thread (re-use empty location
		 * in array). */
		datalooper = (*env)->NewGlobalRef(env, looper);
		if((datalooper == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
			pthread_mutex_unlock(&mutex);
			throw_serialcom_exception(env, 3, 0, E_NEWGLOBALREFSTR);
			return -1;
		}
		params.jvm = jvm;
		params.fd = fd;
		params.looper = datalooper;
		params.data_thread_id = 0;
		params.event_thread_id = 0;
		params.evfd = 0;
		params.data_thread_exit = 0;
		params.event_thread_exit = 0;
		params.mutex = &mutex;
		params.data_init_done = -1;
		params.event_init_done = -1;
		params.event_custom_err_code = 0;
		params.event_standard_err_code = 0;
		params.data_custom_err_code = 0;
		params.data_standard_err_code = 0;
		params.data_cond_var = cond_var;
		fd_looper_info[x] = params;
		arg = &fd_looper_info[x];
	}else {
		/* Set the values, create reference to it to be passed to thread (very first initialization).*/
		datalooper = (*env)->NewGlobalRef(env, looper);
		if((datalooper == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
			pthread_mutex_unlock(&mutex);
			throw_serialcom_exception(env, 3, 0, E_NEWGLOBALREFSTR);
			return -1;
		}
		params.jvm = jvm;
		params.fd = fd;
		params.looper = datalooper;
		params.data_thread_id = 0;
		params.event_thread_id = 0;
		params.evfd = 0;
		params.data_thread_exit = 0;
		params.event_thread_exit = 0;
		params.mutex = &mutex;
		params.data_init_done = -1;
		params.event_init_done = -1;
		params.event_custom_err_code = 0;
		params.event_standard_err_code = 0;
		params.data_custom_err_code = 0;
		params.data_standard_err_code = 0;
		params.data_cond_var = cond_var;
		fd_looper_info[dtp_index] = params;
		arg = &fd_looper_info[dtp_index];
	}

	pthread_attr_init(&((struct com_thread_params*) arg)->data_thread_attr);
	pthread_attr_setdetachstate(&((struct com_thread_params*) arg)->data_thread_attr, PTHREAD_CREATE_JOINABLE);
	ret = pthread_create(&thread_id, NULL, &data_looper, arg);
	if(ret != 0) {
		(*env)->DeleteGlobalRef(env, datalooper);
		pthread_attr_destroy(&((struct com_thread_params*) arg)->data_thread_attr);
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	if((entry_found == JNI_TRUE) || (empty_entry_found == JNI_TRUE)) {
		/* index has been already incremented when data looper thread was created, so do nothing. */
	}else {
		/* update address where parameters for next thread will be stored. */
		dtp_index++;
	}

	/* wait till worker thread initializes completely. */
	if(-1 == ((struct com_thread_params*) arg)->data_init_done) {
		/* pthread_cond_wait() automatically and atomically unlocks the associated mutex variable
		 * so that it can be used by worker thread */
		pthread_cond_wait(&(((struct com_thread_params*) arg)->data_cond_var), &mutex);
	}

	pthread_mutex_unlock(&mutex);
	pthread_cond_destroy(&(((struct com_thread_params*) arg)->data_cond_var));

	if(0 == ((struct com_thread_params*) arg)->data_init_done) {
		/* Save the data thread id which will be used when listener is unregistered. */
		((struct com_thread_params*) arg)->data_thread_id = thread_id;
	}else {
		(*env)->DeleteGlobalRef(env, datalooper);
		pthread_attr_destroy(&((struct com_thread_params*) arg)->data_thread_attr);
		((struct com_thread_params*) arg)->data_thread_id = 0;

		if((((struct com_thread_params*) arg)->data_custom_err_code) > 0) {
			/* indicates custom error message should be used in exception.*/
			throw_serialcom_exception(env, 2, ((struct com_thread_params*) arg)->data_custom_err_code, NULL);
		}else {
			/* indicates posix/os-specific error message should be used in exception.*/
			throw_serialcom_exception(env, 1, ((struct com_thread_params*) arg)->data_standard_err_code, NULL);
		}
		return -1;
	}

	return 0; /* success */
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    destroyDataLooperThread
 * Signature: (J)I
 *
 * Terminates native thread.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_destroyDataLooperThread(JNIEnv *env,
		jobject obj, jlong fd) {

	int ret = -1;
	int x = -1;
	struct com_thread_params *ptr = NULL;
	pthread_t data_thread_id = 0;
	void *status = NULL;

#if defined (__linux__)
	uint64_t value = 1;
#endif

	ptr = fd_looper_info;
	pthread_mutex_lock(&mutex);

	/* Find the data thread serving this file descriptor. */
	for (x=0; x < MAX_NUM_THREADS; x++) {
		if(ptr->fd == fd) {
			data_thread_id = ptr->data_thread_id;
			break;
		}
		ptr++;
	}

	/* Set the flag that will be checked by thread when it comes out of waiting state. */
	ptr->data_thread_exit = 1;

#if defined (__linux__)
	/* If the data looper thread is waiting for an event, let us cause an event to happen,
	 * so thread come out of waiting on fd and can check thread_exit flag. */
	ret = write(ptr->evfd, &value, sizeof(value));
#elif defined (__APPLE__) || defined (__SunOS)
	ret = write(ptr->evfd, "E", strlen("E"));
#endif

	/* Join the thread to check its exit status. */
	ret = pthread_join(data_thread_id, &status);
	if(ret != 0) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	ret = pthread_attr_destroy(&(ptr->data_thread_attr));
	if(ret != 0) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	ptr->data_thread_id = 0;   /* Reset thread id field. */

	/* If neither data nor event thread exist for this file descriptor remove entry for it from
	 * global array. Free/delete global reference for looper object as well. */
	if(ptr->event_thread_id == 0) {
		ptr->fd = -1;
		(*env)->DeleteGlobalRef(env, ptr->looper);
	}

	pthread_mutex_unlock(&mutex);
	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    setUpEventLooperThread
 * Signature: (JLcom/embeddedunveiled/serial/internal/SerialComLooper;)I
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_setUpEventLooperThread(JNIEnv *env,
		jobject obj, jlong fd, jobject looper) {

	int ret = -1;
	int x = -1;
	struct com_thread_params *ptr = NULL;
	jboolean entry_found = JNI_FALSE;
	jboolean empty_entry_found = JNI_FALSE;
	pthread_t thread_id;
	struct com_thread_params params;
	jobject eventlooper = NULL;
	void *arg;
	pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

	ptr = fd_looper_info;

	/* we make sure that thread creation, data passing and access to global data is atomic. */
	pthread_mutex_lock(&mutex);

	/* Check if there is an entry for this fd already in global array. If yes, we will update that
	 * with information about data thread. Further if there is an unused index we will reuse it. */
	for (x=0; x < MAX_NUM_THREADS; x++) {
		if((ptr->fd == fd) || (ptr->fd == -1)) {
			if(ptr->fd == fd) {
				entry_found = JNI_TRUE;
			}
			if(ptr->fd == -1) {
				empty_entry_found = JNI_TRUE;
			}
			break;
		}
		ptr++;
	}

	if((entry_found == JNI_TRUE) && (empty_entry_found == JNI_FALSE)) {
		/* Set up pointer to location which will be passed to thread. */
		ptr->event_init_done = -1;
		ptr->event_standard_err_code = 0;
		ptr->event_custom_err_code = 0;
		ptr->event_cond_var = cond_var;
		arg = &fd_looper_info[x];
	}else if((entry_found == JNI_FALSE) && (empty_entry_found == JNI_TRUE)) {
		/* Set the values, create reference to it to be passed to thread. */
		eventlooper = (*env)->NewGlobalRef(env, looper);
		if((eventlooper == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
			pthread_mutex_unlock(&mutex);
			throw_serialcom_exception(env, 3, 0, E_NEWGLOBALREFSTR);
			return -1;
		}
		params.jvm = jvm;
		params.fd = fd;
		params.looper = eventlooper;
		params.data_thread_id = 0;
		params.event_thread_id = 0;
		params.evfd = 0;
		params.data_thread_exit = 0;
		params.event_thread_exit = 0;
		params.mutex = &mutex;
		params.data_init_done = -1;
		params.event_init_done = -1;
		params.event_custom_err_code = 0;
		params.event_standard_err_code = 0;
		params.data_custom_err_code = 0;
		params.data_standard_err_code = 0;
		params.event_cond_var = cond_var;
		fd_looper_info[x] = params;
		arg = &fd_looper_info[x];
	}else {
		/* Set the values, create reference to it to be passed to thread. */
		eventlooper = (*env)->NewGlobalRef(env, looper);
		if((eventlooper == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
			pthread_mutex_unlock(&mutex);
			throw_serialcom_exception(env, 3, 0, E_NEWGLOBALREFSTR);
			return -1;
		}
		params.jvm = jvm;
		params.fd = fd;
		params.looper = eventlooper;
		params.data_thread_id = 0;
		params.event_thread_id = 0;
		params.evfd = 0;
		params.data_thread_exit = 0;
		params.event_thread_exit = 0;
		params.mutex = &mutex;
		params.data_init_done = -1;
		params.event_init_done = -1;
		params.event_custom_err_code = 0;
		params.event_standard_err_code = 0;
		params.data_custom_err_code = 0;
		params.data_standard_err_code = 0;
		params.event_cond_var = cond_var;
		fd_looper_info[dtp_index] = params;
		arg = &fd_looper_info[dtp_index];
	}

	pthread_attr_init(&((struct com_thread_params*) arg)->event_thread_attr);
	pthread_attr_setdetachstate(&((struct com_thread_params*) arg)->event_thread_attr, PTHREAD_CREATE_JOINABLE);
	ret = pthread_create(&thread_id, NULL, &event_looper, arg);
	if(ret != 0) {
		(*env)->DeleteGlobalRef(env, eventlooper);
		pthread_attr_destroy(&((struct com_thread_params*) arg)->event_thread_attr);
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	if((entry_found == JNI_TRUE) || (empty_entry_found == JNI_TRUE)) {
		/* index has been already incremented when data looper thread was created, so do nothing. */
	}else {
		/* update address where parameters for next thread will be stored. */
		dtp_index++;
	}

	/* wait till worker thread initializes completely. */
	if(-1 == ((struct com_thread_params*) arg)->event_init_done) {
		pthread_cond_wait(&(((struct com_thread_params*) arg)->event_cond_var), &mutex);
	}

	pthread_mutex_unlock(&mutex);
	pthread_cond_destroy(&(((struct com_thread_params*) arg)->event_cond_var));

	if(0 == ((struct com_thread_params*) arg)->event_init_done) {
		/* Save the data thread id which will be used when listener is unregistered. */
		((struct com_thread_params*) arg)->event_thread_id = thread_id;
	}else {
		(*env)->DeleteGlobalRef(env, eventlooper);
		pthread_attr_destroy(&((struct com_thread_params*) arg)->event_thread_attr);
		((struct com_thread_params*) arg)->event_thread_id = 0;

		if((((struct com_thread_params*) arg)->event_custom_err_code) > 0) {
			/* indicates custom error message should be used in exception.*/
			throw_serialcom_exception(env, 2, ((struct com_thread_params*) arg)->event_custom_err_code, NULL);
		}else {
			/* indicates posix/os-specific error message should be used in exception.*/
			throw_serialcom_exception(env, 1, ((struct com_thread_params*) arg)->event_standard_err_code, NULL);
		}
		return -1;
	}

	return 0; /* success */
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    destroyEventLooperThread
 * Signature: (J)I
 *
 * Terminates the event looper worker thread.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_SerialComJNINativeInterface_destroyEventLooperThread(JNIEnv *env,
		jobject obj, jlong fd) {

	int ret = -1;
	int x = -1;
	struct com_thread_params *ptr = NULL;
	pthread_t event_thread_id = 0;
	void *status;

	ptr = fd_looper_info;
	pthread_mutex_lock(&mutex);

	/* Find the event thread serving this file descriptor. */
	for (x=0; x < MAX_NUM_THREADS; x++) {
		if(ptr->fd == fd) {
			event_thread_id = ptr->event_thread_id;
			break;
		}
		ptr++;
	}

	/* Set the flag that will be checked by thread when it comes out of waiting state. */
	ptr->event_thread_exit = 1;

	/* send signal to event thread. */
	ret = pthread_kill(event_thread_id, SIGUSR1);
	if(ret != 0) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	/* Join the thread (waits for the thread specified to terminate). */
	ret = pthread_join(event_thread_id, &status);
	if(ret != 0) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	ret = pthread_attr_destroy(&(ptr->event_thread_attr));
	if(ret != 0) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	ptr->event_thread_id = 0;    /* Reset thread id field. */

	/* If neither data nor event thread exist for this file descriptor remove entry for it from global array. */
	if(ptr->data_thread_id == 0) {
		ptr->fd = -1;
		(*env)->DeleteGlobalRef(env, ptr->looper);
	}

	pthread_mutex_unlock(&mutex);
	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    registerUSBHotPlugEventListener
 * Signature: (Lcom/embeddedunveiled/serial/ISerialComUSBHotPlugListener;IILjava/lang/String;)I
 *
 * Create a native thread that works with operating system specific mechanism for USB hot plug
 * facility.
 *
 * @return index of an array at which information about this worker thread is saved on success
 *         otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_registerUSBHotPlugEventListener(JNIEnv *env,
		jobject obj, jobject hotPlugListener, jint filterVID, jint filterPID, jstring serialNumber) {

	int ret = -1;
	int x = 0;
	int empty_index_found = 0;
	struct usb_dev_monitor_info *ptr = NULL;
	pthread_t thread_id = 0;
	void *arg = NULL;
	struct usb_dev_monitor_info params;
	jobject usbHotPlugListener = NULL;
	const char* serial_number = NULL;
	pthread_cond_t condvar = PTHREAD_COND_INITIALIZER;

	ptr = &usb_hotplug_monitor_info[0];
	pthread_mutex_lock(&mutex);

	usbHotPlugListener = (*env)->NewGlobalRef(env, hotPlugListener);
	if(usbHotPlugListener == NULL || ((*env)->ExceptionOccurred(env) != NULL)) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 3, 0, E_NEWGLOBALREFSTR);
		return -1;
	}

	if(serialNumber != NULL) {
		serial_number = (*env)->GetStringUTFChars(env, serialNumber, NULL);
		if((serial_number == NULL) || ((*env)->ExceptionOccurred(env) != NULL)) {
			throw_serialcom_exception(env, 3, 0, E_GETSTRUTFCHARSTR);
			return -1;
		}
	}

	/* Check if there is an unused index then we will reuse it. */
	for (x=0; x < MAX_NUM_THREADS; x++) {
		if(ptr->thread_id == 0) {
			empty_index_found = 1;
			break;
		}
		ptr++;
	}

	params.jvm = jvm;
	params.usbHotPlugEventListener = usbHotPlugListener;
	params.usb_vid_to_match = filterVID;
	params.usb_pid_to_match = filterPID;
	if(serialNumber != NULL) {
		memset(params.serial_number_to_match, '\0', sizeof(params.serial_number_to_match));
		strcpy(params.serial_number_to_match, serial_number);
	}else {
		params.serial_number_to_match[0] = '\0';
	}
	params.thread_exit = 0;
	params.init_done = -1;
	params.custom_err_code = 0;
	params.standard_err_code = 0;
	params.cond_var = condvar;
	params.mutex = &mutex;
#if defined (__linux__)
	params.evfd = 0;
#endif
#if defined (__APPLE__)
	params.empty_iterator_added = 0;
	params.empty_iterator_removed = 0;
	if(empty_index_found == 1) {
		params.data = &usb_hotplug_monitor_info[x];
	}else {
		params.data = &usb_hotplug_monitor_info[usb_dev_monitor_index];
	}
#endif
	if(empty_index_found == 1) {
		usb_hotplug_monitor_info[x] = params;
		arg = &usb_hotplug_monitor_info[x];
	}else {
		usb_hotplug_monitor_info[usb_dev_monitor_index] = params;
		arg = &usb_hotplug_monitor_info[usb_dev_monitor_index];
	}

	pthread_attr_init(&((struct usb_dev_monitor_info*) arg)->thread_attr);
	pthread_attr_setdetachstate(&((struct usb_dev_monitor_info*) arg)->thread_attr, PTHREAD_CREATE_JOINABLE);
	ret = pthread_create(&thread_id, NULL, &usb_device_hotplug_monitor, arg);
	if(ret != 0) {
		(*env)->DeleteGlobalRef(env, usbHotPlugListener);
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	/* wait till worker thread initializes completely. */
	if(((struct usb_dev_monitor_info*) arg)->init_done == -1) {
		/* pthread_cond_wait() automatically and atomically unlocks the associated mutex variable
		 * so that it can be used by worker thread */
		pthread_cond_wait(&(((struct usb_dev_monitor_info*) arg)->cond_var), &mutex);
	}

	pthread_mutex_unlock(&mutex);
	pthread_cond_destroy(&(((struct usb_dev_monitor_info*) arg)->cond_var));

	if(0 == ((struct usb_dev_monitor_info*) arg)->init_done) {
		/* Save the thread id which will be used when listener is unregistered. */
		((struct usb_dev_monitor_info*) arg)->thread_id = thread_id;
		if(empty_index_found == 1) {
			return x;
		}else {
			/* update index where data for next thread will be saved. */
			usb_dev_monitor_index++;
			return (usb_dev_monitor_index - 1);
		}
	}else {
		(*env)->DeleteGlobalRef(env, usbHotPlugListener);
		pthread_attr_destroy(&((struct usb_dev_monitor_info*) arg)->thread_attr);
		((struct usb_dev_monitor_info*) arg)->thread_id = 0;

		if((((struct usb_dev_monitor_info*) arg)->custom_err_code) > 0) {
			/* indicates custom error message should be used in exception.*/
			throw_serialcom_exception(env, 2, ((struct usb_dev_monitor_info*) arg)->custom_err_code, NULL);
		}else {
			/* indicates posix/os-specific error message should be used in exception.*/
			throw_serialcom_exception(env, 1, ((struct usb_dev_monitor_info*) arg)->standard_err_code, NULL);
		}
		return -1;
	}

	/* should not be reached */
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    unregisterUSBHotPlugEventListener
 * Signature: (I)I
 *
 * Destroy worker thread used for USB hot plug monitoring. The java layer sends index in array
 * where info about the native thread to be destroyed is stored.
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_unregisterUSBHotPlugEventListener(JNIEnv *env,
		jobject obj, jint indexOfArrayElement) {

#if defined (__linux__) || defined (__APPLE__)
	int ret = -1;
	void *status = NULL;
	struct usb_dev_monitor_info *ptr = &usb_hotplug_monitor_info[indexOfArrayElement];

#if defined (__linux__)
	ssize_t size;
	uint64_t value = 34;
#endif

	pthread_mutex_lock(&mutex);

	/* Set the flag that will be checked by thread to check for exit condition. */
	ptr->thread_exit = 1;

#if defined (__linux__)
	/* make pselect come out of waiting state through event on evfd. */
	errno = 0;
	size = write(ptr->evfd, &value, sizeof(value));
	if(size != sizeof(uint64_t)) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
#endif
#if defined (__APPLE__)
	/* tell run loop to come out of waiting state and exit looping. */
	CFRunLoopSourceSignal(ptr->exit_run_loop_source);
	CFRunLoopWakeUp(ptr->run_loop);
#endif

	/* Join the thread (waits for the thread specified to terminate). */
	ret = pthread_join(ptr->thread_id, &status);
	if(ret != 0) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	(*env)->DeleteGlobalRef(env, ptr->usbHotPlugEventListener);
	ret = pthread_attr_destroy(&(ptr->thread_attr));
	if(ret != 0) {
		pthread_mutex_unlock(&mutex);
		throw_serialcom_exception(env, 1, ret, NULL);
		return -1;
	}

	/* Reset thread id field. */
	ptr->thread_id = 0;

	pthread_mutex_unlock(&mutex);
#endif
#if defined (__SunOS)
#endif

	return 0; /* success */
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    pauseListeningEvents
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_pauseListeningEvents(JNIEnv *env,
		jobject obj, jlong fd) {
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    resumeListeningEvents
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_resumeListeningEvents(JNIEnv *env,
		jobject obj, jlong fd) {
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    ioctlExecuteOperation
 * Signature: (JJ)J
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jlong JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_ioctlExecuteOperation(JNIEnv *env,
		jobject obj, jlong fd, jlong operationCode) {
	int ret = 0;

	errno = 0;
	ret = ioctl(fd, operationCode);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    ioctlSetValue
 * Signature: (JJJ)J
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jlong JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_ioctlSetValue(JNIEnv *env,
		jobject obj, jlong fd, jlong operationCode, jlong value) {
	int ret = 0;

	errno = 0;
	ret = ioctl(fd, operationCode, value);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
	return 0;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    ioctlGetValue
 * Signature: (JJ)J
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jlong JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_ioctlGetValue(JNIEnv *env,
		jobject obj, jlong fd, jlong operationCode) {
	int ret = 0;
	long value = 0;

	errno = 0;
	ret = ioctl(fd, operationCode, &value);
	if(ret < 0) {
		throw_serialcom_exception(env, 1, errno, NULL);
		return -1;
	}
	return value;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    ioctlSetValueIntArray
 * Signature: (JJ[I)J
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jlong JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_ioctlSetValueIntArray(JNIEnv *env,
		jobject obj, jlong v, jlong f, jintArray r) {
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    ioctlSetValueCharArray
 * Signature: (JJ[B)J
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jlong JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_ioctlSetValueCharArray(JNIEnv *env,
		jobject obj, jlong q, jlong c, jbyteArray v) {
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getCDCUSBDevPowerInfo
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobjectArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getCDCUSBDevPowerInfo
(JNIEnv *env, jobject obj, jstring comPortName) {
	return get_usbdev_powerinfo(env, comPortName);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    setLatencyTimer
 * Signature: (Ljava/lang/String;B)I
 *
 * @return 0 on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_setLatencyTimer
(JNIEnv *env, jobject obj, jstring comPortName, jbyte timerValue) {
	return set_latency_timer_value(env, comPortName, timerValue);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    getLatencyTimer
 * Signature: (Ljava/lang/String;)I
 *
 * @return currently set latency timer value on success otherwise -1 if an error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_getLatencyTimer
(JNIEnv *env, jobject obj, jstring comPortName) {
	return get_latency_timer_value(env, comPortName);
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    rescanUSBDevicesHW
 * Signature: ()I
 *
 * Applicable to Windows operating system only.
 *
 * @return -1 always.
 */
JNIEXPORT jint JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_rescanUSBDevicesHW(JNIEnv *env,
		jobject obj) {
	return -1;
}

/*
 * Class:     com_embeddedunveiled_serial_internal_SerialComPortJNIBridge
 * Method:    listBTSPPDevNodesWithInfo
 * Signature: ()[Ljava/lang/String;
 *
 * @return array of Strings containing info about rfcomm device nodes found, NULL if error occurs.
 * @throws SerialComException if any JNI function, system call or C function fails.
 */
JNIEXPORT jobjectArray JNICALL Java_com_embeddedunveiled_serial_internal_SerialComPortJNIBridge_listBTSPPDevNodesWithInfo(JNIEnv *env,
		jobject obj) {
	return list_bt_rfcomm_dev_nodes(env);
}

#endif /* End compiling for Unix-like OS. */

