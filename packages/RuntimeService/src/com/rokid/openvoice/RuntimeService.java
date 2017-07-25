package com.rokid.openvoice;

import android.app.Service;
import android.content.Intent;
import android.content.Context;
import android.os.IBinder;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.net.NetworkInfo;
import android.net.ConnectivityManager;

public class RuntimeService extends Service{

	String TAG = getClass().getSimpleName();

	public RuntimeNative mRuntimeNative = null;
    public static MainHandler mHandler = null;
    public static final int MSG_REINIT = 1;

	public static boolean initialized = false;

	public static final int SIREN_STATE_AWAKE = 1;
	public static final int SIREN_STATE_SLEEP = 2;

    private static final int EVENT_VAD_ATART = 100;
    private static final int EVENT_VAD_DATA = 101;
    private static final int EVENT_VAD_END = 102;
    private static final int EVENT_VAD_CANCEL = 103;
    private static final int EVENT_WAKE_NOCMD = 108;
    private static final int EVENT_WAKE_CMD = 109;
    private static final int EVENT_SLEEP = 111;

    class MainHandler extends Handler{

        public void handleMessage(Message msg) {
            switch(msg.what){
                case MSG_REINIT:
                    reinit();
                    break;
            }
        }
    }

	public RuntimeService(){
		Log.e(TAG, "RuntimeService  created ");
		mRuntimeNative = RuntimeNative.asInstance();
		mRuntimeNative.init();
		mRuntimeNative.addBinder(proxy);
		initialized = true;
	}

	@Override
	public void onCreate(){
        mHandler = new MainHandler();
		ConnectivityManager cm = (ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo mNetworkInfo = cm.getActiveNetworkInfo();
		if(mNetworkInfo != null){
		    mRuntimeNative.networkStateChange(true);
		}
		mUEventObserver.startObserving("/sound/card1/pcmC1D0c");
	}

    private void reinit(){
        Log.e(TAG, "==========================REINITT=============================");
		mRuntimeNative = RuntimeNative.asInstance();
		mRuntimeNative.init();
		mRuntimeNative.addBinder(proxy);
        initialized = true;
		ConnectivityManager cm = (ConnectivityManager)getSystemService(Context.CONNECTIVITY_SERVICE);
		NetworkInfo mNetworkInfo = cm.getActiveNetworkInfo();
		if(mNetworkInfo != null){
		    mRuntimeNative.networkStateChange(true);
		}
    }

	private final IRuntimeService.Stub proxy = new IRuntimeService.Stub(){

		@Override
		public void onVoiceCommand(String asr, String nlp, String action){
			mRuntimeNative.setSirenState(SIREN_STATE_AWAKE);
            Log.e(TAG, "asr\t" + asr);
            Log.e(TAG, "nlp\t" + nlp);
            Log.e(TAG, "action " + action);
		}

		@Override
		public void onVoiceEvent(int event, double sl_degree, int has_sl, double energy, double threshold){
			Log.e(TAG, event+" ,has_sl : " + has_sl + " ,sl_degree : " + (float)sl_degree);
			if(event == EVENT_VAD_ATART){

			}else if(event == EVENT_VAD_END || evnet == EVENT_VAD_CANCEL){
				mRuntimeNative.setSirenState(SIREN_STATE_SLEEP);
			}
		}

        @Override
        public void onVoiceReject(){

        }

        @Override
        public void onSpeechTimeout(){

        }
	};

	private final android.os.UEventObserver mUEventObserver = new android.os.UEventObserver() {
		 
		@Override
		public void onUEvent(android.os.UEventObserver.UEvent event){
			Log.e(TAG, event.toString());
            if(initialized){
    			String action = event.get("ACTION");
    			if("add".equals(action)){
    				mRuntimeNative.startSiren(true);	
    			}else if("remove".equals(action)){
    				mRuntimeNative.startSiren(false);
    			}
            }
		}
	};

	@Override
	public IBinder onBind(Intent intent) {
		return null;
	}
}
