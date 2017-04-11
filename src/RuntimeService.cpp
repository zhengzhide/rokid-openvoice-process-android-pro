#define LOG_TAG "RuntimeService"
#define LOG_NDEBUG 0

#include <stdio.h>
#include <binder/IServiceManager.h>

#include "RuntimeService.h"
#include "voice_engine.h"
#include "json.h"

using namespace android;
using namespace std;
using namespace rokid;
using namespace speech;

bool RuntimeService::init(){
	if(!_init(this)){
		ALOGE("init siren failed.");
		return false;
	}
	pthread_create(&event_thread, NULL, onEvent, this);
	return true;
}

void RuntimeService::start_siren(bool flag){
	this->flag = flag;
	set_siren_state_change((flag ? SIREN_STATE_AWAKE : SIREN_STATE_SLEEP));
}

void RuntimeService::set_siren_state(const int &state){
	set_siren_state_change(state);
	ALOGV("current_status     >>   %d", state);
}

void RuntimeService::config(){
	json_object *json_obj = json_object_from_file(SPEECH_CONFIG_FILE);
	
	if(json_obj == NULL) {
		ALOGE("%s not find", SPEECH_CONFIG_FILE);
	}

	json_object *server_address = NULL;
	json_object *ssl_roots_pem = NULL;
	json_object *auth_key = NULL;
	json_object *device_type = NULL;
	json_object *device_id = NULL;
	json_object *secret = NULL;
	json_object *api_version = NULL;
	json_object *codec = NULL;

	if(TRUE == json_object_object_get_ex(json_obj, "server_address", &server_address)){
		_speech->config("server_address", json_object_get_string(server_address));
		ALOGE("%s", json_object_get_string(server_address));
		json_object_put(server_address);
	}
	if(TRUE == json_object_object_get_ex(json_obj, "ssl_roots_pem", &ssl_roots_pem)){
		_speech->config("ssl_roots_pem", json_object_get_string(ssl_roots_pem));
		ALOGE("%s", json_object_get_string(ssl_roots_pem));
		json_object_put(ssl_roots_pem);
	}
	if(TRUE == json_object_object_get_ex(json_obj, "auth_key", &auth_key)){
		_speech->config("auth_key", json_object_get_string(auth_key));
		ALOGE("%s", json_object_get_string(auth_key));
		json_object_put(auth_key);
	}
	if(TRUE == json_object_object_get_ex(json_obj, "device_type", &device_type)){
		_speech->config("device_type", json_object_get_string(device_type));
		ALOGE("%s", json_object_get_string(device_type));
		json_object_put(device_type);
	}
	if(TRUE == json_object_object_get_ex(json_obj, "device_id", &device_id)){
		_speech->config("device_id", json_object_get_string(device_id));
		ALOGE("%s", json_object_get_string(device_id));
		json_object_put(device_id);
	}
	if(TRUE == json_object_object_get_ex(json_obj, "api_version", &api_version)){
		_speech->config("api_version", json_object_get_string(api_version));
		ALOGE("%s", json_object_get_string(api_version));
		json_object_put(api_version);
	}
	if(TRUE == json_object_object_get_ex(json_obj, "secret", &secret)){
		_speech->config("secret", json_object_get_string(secret));
		ALOGE("%s", json_object_get_string(secret));
		json_object_put(secret);
	}
	if(TRUE == json_object_object_get_ex(json_obj, "codec", &codec)){
		_speech->config("codec", json_object_get_string(codec));
		ALOGE("%s", json_object_get_string(codec));
		json_object_put(codec);
	}
	_speech->config("vt", "若琪");
	free(json_obj);
//	json_object_put(json_obj);
}

void* onEvent(void* arg){
	ALOGV("thread join !!");
	RuntimeService *runtime = (RuntimeService*)arg;
	sp<IBinder> binder = defaultServiceManager()->getService(String16("rk_power_manager")); 

	int id = -1;
	bool err = false;;
	runtime->_speech = new_speech();
	runtime->config();
	if (!runtime->_speech->prepare()) {
		ALOGE("=========prepare failed===============");
		return NULL;
	}
	//FILE *fd = fopen("/data/voice.pcm", "w");
	pthread_create(&runtime->response_thread, NULL, onResponse, runtime);
	for(;;){
		pthread_mutex_lock(&runtime->event_mutex);
		while(runtime->message_queue.empty()){
			pthread_cond_wait(&runtime->event_cond, &runtime->event_mutex);
		}
		const RuntimeService::VoiceMessage *message = runtime->message_queue.front();
		ALOGV("event : -------------------------%d----", message->event);
		switch(message->event){
			case SIREN_EVENT_WAKE_CMD:
				set_siren_state_change(1);
				ALOGV("voice event   >>>   wake_cmd");
				break;
			case SIREN_EVENT_WAKE_NOCMD:
				ALOGV("voice event   >>>   wake_nocmd");
				break;
			case SIREN_EVENT_SLEEP:
				set_siren_state_change(2);
				ALOGV("voice event   >>>   sleep");
				break;
			case SIREN_EVENT_VAD_START:
			case SIREN_EVENT_WAKE_VAD_START:
				id = runtime->_speech->start_voice();
				ALOGV("voice event   >>   start   id   :  <<%d>>     err : <<%d>>", id,  err);
				break;
			case SIREN_EVENT_VAD_DATA:
			case SIREN_EVENT_WAKE_VAD_DATA:
				if (id > 0 && message->has_voice > 0) {
					runtime->_speech->put_voice(id, (uint8_t *)message->buff, message->length);
					//fwrite(message->buff, message->length, 1, fd);
				}
				break;
			case SIREN_EVENT_VAD_END:
			case SIREN_EVENT_WAKE_VAD_END:
				ALOGV("voice event : end   id    >>>   %d ",id);
				if(binder != NULL){
					Parcel data, reply;
					data.writeInterfaceToken(String16("com.rokid.server.RKPowerManager"));
					data.writeInt32(0x1005);
					data.writeInt32(0);
					binder->transact(IBinder::FIRST_CALL_TRANSACTION, data, &reply);
					reply.readExceptionCode();
				}else{
					ALOGI("power manager is null");
				}
				if(id > 0) {
					runtime->_speech->end_voice(id);
					//fclose(fd);
				}
				break;
			case SIREN_EVENT_VAD_CANCEL:
			case SIREN_EVENT_WAKE_CANCEL:
				if(id > 0)
					runtime->_speech->cancel(id);
				ALOGI("voice event : cancel   id    >>>    %d", id);
				break;
			case SIREN_EVENT_WAKE_PRE:
				ALOGV("vicee event  >>>   prepare");
				break;
		}
		runtime->message_queue.pop_front();
		delete message;
		pthread_mutex_unlock(&runtime->event_mutex);
	}
	runtime->_speech->release();
	delete runtime->_speech;
	ALOGV("thread quit!");
	return NULL;
}

void* onResponse(void* arg){
	RuntimeService *runtime= (RuntimeService*)arg;
	json_object *_json_obj = NULL;
	SpeechResult sr;
	sp<IBinder> binder = defaultServiceManager()->getService(String16("runtime_java"));
	for(;;){
		int32_t flag = runtime->_speech->poll(sr);
		if (flag < 0)
			break;

		ALOGV("result : asr  >>  %s    %d", sr.asr.c_str(), flag);
		ALOGV("result : nlp  >>  %s", sr.nlp.c_str());
		ALOGV("result : action >>  %s", sr.action.c_str());

		if(flag == 0 && sr.nlp != ""){
			_json_obj = json_tokener_parse("{}");
			json_object_object_add(_json_obj, "nlp", json_object_new_string(sr.nlp.c_str()));
			json_object_object_add(_json_obj, "asr", json_object_new_string(sr.asr.c_str()));
			json_object_object_add(_json_obj, "action", json_object_new_string(sr.action.c_str()));

			json_object *nlp_obj = json_tokener_parse(sr.nlp.c_str());
			json_object *cdomain_obj = NULL;
			if(TRUE == json_object_object_get_ex(nlp_obj, "domain", &cdomain_obj)){
				const char *cdomain_str = json_object_get_string(cdomain_obj);
				string s(const_cast<char *>(cdomain_str));
				if(s.find("ROKID.EXCEPTION") == std::string::npos){
					runtime->_speech->config("cdomain", cdomain_str);
				}else{
					runtime->_speech->config("cdomain", "");
				}
				json_object_put(cdomain_obj);
				free(nlp_obj);
			}
			ALOGV("-------------------------------------------------------------------------");
			ALOGV("%s", json_object_to_json_string(_json_obj));
			ALOGV("-------------------------------------------------------------------------");
			if(binder != NULL){
				Parcel data, reply;
				data.writeInterfaceToken(String16("rokid.os.IRuntimeService"));
				data.writeString16(String16(json_object_to_json_string(_json_obj)));
				binder->transact(IBinder::FIRST_CALL_TRANSACTION + 0, data, &reply);
				reply.readExceptionCode();
			}else{
				ALOGI("java runtime is null , Waiting for it to initialize");
			}
			free(_json_obj);
			_json_obj = NULL;
		}
	}
	return NULL;
}
