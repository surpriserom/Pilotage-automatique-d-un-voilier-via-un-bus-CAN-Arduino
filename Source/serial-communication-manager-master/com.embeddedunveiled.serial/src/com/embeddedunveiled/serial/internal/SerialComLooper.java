/*
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
 */

package com.embeddedunveiled.serial.internal;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.atomic.AtomicBoolean;

import com.embeddedunveiled.serial.ISerialComDataListener;
import com.embeddedunveiled.serial.ISerialComEventListener;
import com.embeddedunveiled.serial.SerialComDataEvent;
import com.embeddedunveiled.serial.SerialComException;
import com.embeddedunveiled.serial.SerialComLineEvent;
import com.embeddedunveiled.serial.SerialComManager;

/**
 * <p>Encapsulates environment for data and event looper implementation. This runs in as a 
 * different thread context and keep looping over data/event queue, delivering data/events 
 * to the intended registered listener (data/event handler) one by one.</p>
 * 
 * <p>The rate of delivery of data/events are directly proportional to how fast listener finishes
 * his job and let us return.</p>
 * 
 * @author Rishi Gupta
 */
public final class SerialComLooper {

	private final int MAX_NUM_EVENTS = 5000;
	private SerialComPortJNIBridge mComPortJNIBridge = null;
	private SerialComErrorMapper mErrMapper = null;

	private BlockingQueue<SerialComDataEvent> mDataQueue = null;
	private ISerialComDataListener mDataListener = null;
	private Object mDataLock = new Object();
	private Thread mDataLooperThread = null;
	private AtomicBoolean deliverDataEvent = new AtomicBoolean(true);
	private AtomicBoolean exitDataThread = new AtomicBoolean(false);

	private BlockingQueue<Integer> mDataErrorQueue = null;
	private Object mDataErrorLock = new Object();
	private Thread mDataErrorLooperThread = null;
	private AtomicBoolean exitDataErrorThread = new AtomicBoolean(false);

	private BlockingQueue<SerialComLineEvent> mEventQueue = null;
	private ISerialComEventListener mEventListener = null;
	private Thread mEventLooperThread = null;
	private AtomicBoolean exitEventThread = null;

	private int appliedMask = SerialComManager.CTS | SerialComManager.DSR | SerialComManager.DCD | SerialComManager.RI;
	private int oldLineState = 0;
	private int newLineState = 0;

	/**
	 * <p>This class runs in as a different thread context and keep looping over data queue, delivering 
	 * data to the intended registered listener (data handler) one by one. The rate of delivery of
	 * new data is directly proportional to how fast listener finishes his job and let us return.</p>
	 */
	class DataLooper implements Runnable {
		@Override
		public void run() {
			/* take() method blocks if there is no event to deliver. So we don't keep wasting 
			 * CPU cycle in case queue is empty. */
			while(true) {
				synchronized(mDataLock) {
					try {
						mDataListener.onNewSerialDataAvailable(mDataQueue.take());
						if(deliverDataEvent.get() == false) {
							/* Causes the current thread to wait until another thread
							 * invokes the notify method. */
							mDataLock.wait();
						}
					} catch (InterruptedException e) {
						if(exitDataThread.get() == true) {
							break;
						}
					}
				}
			}
			exitDataThread.set(false); // Reset exit flag
			mDataQueue = null;
		}
	}

	/**
	 * <p>This class runs in as a different thread context and keep looping over data error queue, delivering 
	 * error event to the intended registered listener (error data handler) one by one. The rate of delivery of
	 * new error event is directly proportional to how fast listener finishes his job and let us return.</p>
	 */
	class DataErrorLooper implements Runnable {
		@Override
		public void run() {
			while(true) {
				synchronized(mDataErrorLock) {
					try {
						mDataListener.onDataListenerError(mDataErrorQueue.take());
						if(deliverDataEvent.get() == false) {
							mDataErrorLock.wait();
						}
					} catch (InterruptedException e) {
						if(exitDataErrorThread.get() == true) {
							break;
						}
					}
				}
			}
			exitDataErrorThread.set(false); // Reset exit flag
			mDataErrorQueue = null;
		}
	}

	/**
	 * <p>This class runs in as a different thread context and keep looping over event queue, delivering 
	 * events to the intended registered listener (event handler) one by one. The rate of delivery of
	 * events are directly proportional to how fast listener finishes his job and let us return.</p>
	 */
	class EventLooper implements Runnable {
		@Override
		public void run() {
			while(true) {
					try {
						mEventListener.onNewSerialEvent(mEventQueue.take());
					} catch (InterruptedException e) {
						if(exitEventThread.get() == true) {
							break;
						}
					}
			}
			exitEventThread.set(false); // Reset exit flag
			mEventQueue = null;
		}
	}

	/**
	 * <p>Allocates a new SerialComLooper object.</p>
	 * @param mComPortJNIBridge interface used to invoke appropriate native function
	 * @param errMapper reference to errMapper object to get and map error information
	 */
	public SerialComLooper(SerialComPortJNIBridge mComPortJNIBridge, SerialComErrorMapper errMapper) { 
		this.mComPortJNIBridge = mComPortJNIBridge;
		mErrMapper = errMapper;
	}

	/**
	 * <p>This method is called from native code to pass data bytes.</p>
	 * @param newData byte array containing data read from serial port
	 */
	public void insertInDataQueue(byte[] newData) {
		if(mDataQueue.remainingCapacity() == 0) {
			mDataQueue.poll();
		}
		try {
			mDataQueue.offer(new SerialComDataEvent(newData));
		} catch (Exception e) {
		}
	}
	
	/**
	 * <p>This method insert error info in error queue which will be later delivered to application.</p>
	 * @param errorNum operating system specific error number to be sent to application
	 */
	public void insertInDataErrorQueue(int errorNum) {
		if(mDataErrorQueue.remainingCapacity() == 0) {
			mDataErrorQueue.poll();
		}
		try {
			mDataErrorQueue.offer(errorNum);
		} catch (Exception e) {
		}
	}

	/**
	 * <p>Native side detects the change in status of lines, get the new line status and call this method. Based on the
	 * mask this method determines whether this event should be sent to application or not.</p>
	 * @param newEvent bit mask representing event on serial port control lines
	 */
	public void insertInEventQueue(int newEvent) {
		newLineState = newEvent & appliedMask;
		if(mEventQueue.remainingCapacity() == 0) {
			mEventQueue.poll();
		}
		try {
			mEventQueue.offer(new SerialComLineEvent(oldLineState, newLineState));
		} catch (Exception e) {
		}
		oldLineState = newLineState;
	}

	/**
	 * <p>Start the thread to loop over data queue. </p>
	 * @param handle handle of the opened port for which data looper need to be started
	 * @param dataListener listener to which data will be delivered
	 * @param portName name of port represented by this handle
	 */
	public void startDataLooper(long handle, ISerialComDataListener dataListener, String portName) {
		try {
			mDataListener = dataListener;
			mDataQueue = new ArrayBlockingQueue<SerialComDataEvent>(MAX_NUM_EVENTS);
			mDataErrorQueue = new ArrayBlockingQueue<Integer>(MAX_NUM_EVENTS);
			mDataLooperThread = new Thread(new DataLooper(), "SCM DataLooper for handle " + handle + " and port " + portName);
			mDataErrorLooperThread = new Thread(new DataErrorLooper(), "SCM DataErrorLooper for handle " + handle + " and port " + portName);
			mDataLooperThread.start();
			mDataErrorLooperThread.start();
		} catch (Exception e) {
		}
	}
	
	/**
	 * <p>Set the flag to indicate that the thread is supposed to run to completion and exit.
	 * Interrupt the thread so that take() method can come out of blocked sleep state.</p>
	 */
	public void stopDataLooper() {
		try {
			exitDataThread.set(true);
			exitDataErrorThread.set(true);
			mDataLooperThread.interrupt();
			mDataErrorLooperThread.interrupt();
		} catch (Exception e) {
		}
	}

	/**
	 * <p>Get initial status of control lines and start thread.</p>
	 * @param handle handle of the opened port for which event looper need to be started
	 * @param eventListener listener to which event will be delivered
	 * @param portName name of port represented by this handle
	 */
	public void startEventLooper(long handle, ISerialComEventListener eventListener, String portName) throws SerialComException {
		int state = 0;
		int[] linestate = new int[8];

		try {
			linestate = mComPortJNIBridge.getLinesStatus(handle);
			if (linestate[0] < 0) {
				throw new SerialComException(mErrMapper.getMappedError(linestate[0]));
			}
			// Bit mask CTS | DSR | DCD | RI
			state = linestate[1] | linestate[2] | linestate[3] | linestate[4];
			oldLineState = state & appliedMask;

			mEventQueue = new ArrayBlockingQueue<SerialComLineEvent>(MAX_NUM_EVENTS);
			exitEventThread = new AtomicBoolean(false);
			mEventListener = eventListener;
			
			mEventLooperThread = new Thread(new EventLooper(), "SCM EventLooper for handle " + handle + " and port " + portName);
			mEventLooperThread.start();
		} catch (Exception e) {
		}
	}

	/**
	 * <p>Set the flag to indicate that the thread is supposed to run to completion and exit.
	 * Interrupt the thread so that take() method can come out of blocked sleep state.</p>
	 */
	public void stopEventLooper() throws SerialComException {
		try {
			exitEventThread.set(true);
			mEventLooperThread.interrupt();
		} catch (Exception e) {
		}
	}

	/**
	 * <p>Data looper thread refrains from sending new data to the data listener.</p>
	 */
	public void pause() {
		deliverDataEvent.set(false);
	}

	/**
	 * <p>Looper starts sending new data again to the data listener.</p>
	 */
	public void resume() {
		deliverDataEvent.set(true);
		mDataLock.notify();
		mDataErrorLock.notify();
	}

	/**
	 * <p>In future we may shift modifying mask in the native code itself, so as to prevent JNI transitions.
	 * This filters what events should be sent to application. Note that, although we sent only those event
	 * for which user has set mask, however native code send all the events to java layer as of now.</p>
	 * @param newMask new bit mask for events that will be delivered to application
	 */
	public void setEventsMask(int newMask) {
		appliedMask = newMask;
	}

	/**
	 * <p>Gives the event mask currently active.</p>
	 * @return bit mask of events currently active
	 */
	public int getEventsMask() {
		return appliedMask;
	}
}