#include <node_buffer.h>

#include "talk/app/webrtc/jsep.h"
#include "webrtc/system_wrappers/interface/ref_count.h"

#include "common.h"
#include "mediastream.h"
#include "mediastreamtrack.h"

using namespace node;
using v8::Boolean;
using v8::External;
using v8::Function;
using v8::Handle;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

Nan::Persistent<Function> MediaStream::constructor;

MediaStream::MediaStream(webrtc::MediaStreamInterface* msi)
: _internalMediaStream(msi)
{
  _inactive = !IsMediaStreamActive();
  uv_mutex_init(&lock);
  uv_async_init(uv_default_loop(), &async, Run);

  async.data = this;
}

MediaStream::~MediaStream()
{
  _internalMediaStream = NULL;
}

NAN_METHOD(MediaStream::New) {
  TRACE_CALL;

  if(!info.IsConstructCall()) {
    return Nan::ThrowTypeError("Use the new operator to construct the MediaStream.");
  }

  Local<External> _msi = Local<External>::Cast(info[0]);
  webrtc::MediaStreamInterface* msi = static_cast<webrtc::MediaStreamInterface*>(_msi->Value());

  MediaStream* obj = new MediaStream(msi);
  msi->RegisterObserver(obj);
  obj->Wrap(info.This());

  TRACE_END;
  info.GetReturnValue().Set(info.This());
}

void MediaStream::QueueEvent(AsyncEventType type, void* data)
{
  TRACE_CALL;
  AsyncEvent evt;
  evt.type = type;
  evt.data = data;
  uv_mutex_lock(&lock);
  _events.push(evt);
  uv_mutex_unlock(&lock);

  uv_async_send(&async);
  TRACE_END;
}

void MediaStream::Run(uv_async_t* handle, int status)
{
  Nan::HandleScope scope;

  MediaStream* self = static_cast<MediaStream*>(handle->data);
  TRACE_CALL_P((uintptr_t)self);
  Local<Object> ms = self->handle();

  while(true)
  {
    uv_mutex_lock(&self->lock);
    bool empty = self->_events.empty();
    if(empty)
    {
      uv_mutex_unlock(&self->lock);
      break;
    }
    AsyncEvent evt = self->_events.front();
    self->_events.pop();
    uv_mutex_unlock(&self->lock);

    TRACE_U("evt.type", evt.type);
    if(MediaStream::CHANGE & evt.type)
    {
      // find current state and send appropriate events
      bool inactive = !self->IsMediaStreamActive();
      if (self->_inactive != inactive)
      {
        if (!inactive)
          evt.type = MediaStream::ACTIVE;
        else
          evt.type = MediaStream::INACTIVE;
        self->_inactive = inactive;
      }
    }

    if(MediaStream::ACTIVE & evt.type)
    {
      Local<Function> callback = Local<Function>::Cast(ms->Get(Nan::New("onactive").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[0];
        Nan::MakeCallback(ms, callback, 0, argv);
      }
    } else if(MediaStream::INACTIVE & evt.type)
    {
      Local<Function> callback = Local<Function>::Cast(ms->Get(Nan::New("oninactive").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[0];
        Nan::MakeCallback(ms, callback, 0, argv);
      }
    }
    if(MediaStream::ADDTRACK & evt.type)
    {
      webrtc::MediaStreamTrackInterface* msti = static_cast<webrtc::MediaStreamTrackInterface*>(evt.data);
      Local<Value> cargv[1];
      cargv[0] = Nan::New<External>(static_cast<void*>(msti));
      Local<Value> mst = Nan::New(MediaStreamTrack::constructor)->NewInstance(1, cargv);
      Local<Function> callback = Local<Function>::Cast(ms->Get(Nan::New("onaddtrack").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[1];
        argv[0] = mst;
        Nan::MakeCallback(ms, callback, 1, argv);
      }
    }
    if(MediaStream::REMOVETRACK & evt.type)
    {
      webrtc::MediaStreamTrackInterface* msti = static_cast<webrtc::MediaStreamTrackInterface*>(evt.data);
      Local<Value> cargv[1];
      cargv[0] = Nan::New<External>(static_cast<void*>(msti));
      Local<Value> mst = Nan::New(MediaStreamTrack::constructor)->NewInstance(1, cargv);
      Local<Function> callback = Local<Function>::Cast(ms->Get(Nan::New("onremovetrack").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[1];
        argv[0] = mst;
        Nan::MakeCallback(ms, callback, 1, argv);
      }
    }
  }
  TRACE_END;
}

bool MediaStream::IsMediaStreamActive()
{
  webrtc::VideoTrackVector videoTracks = _internalMediaStream->GetVideoTracks();
  for (webrtc::VideoTrackVector::iterator track = videoTracks.begin(); track != videoTracks.end(); track++)
  {
    if ((*track)->state() == webrtc::MediaStreamTrackInterface::kLive)
      return true;
  }

  webrtc::AudioTrackVector audioTracks = _internalMediaStream->GetAudioTracks();
  for (webrtc::AudioTrackVector::iterator track = audioTracks.begin(); track != audioTracks.end(); track++)
  {
    if ((*track)->state() == webrtc::MediaStreamTrackInterface::kLive)
      return true;
  }

  return false;
}

void MediaStream::OnChanged()
{
  TRACE_CALL;
  QueueEvent(MediaStream::CHANGE, static_cast<void*>(NULL));
  TRACE_END;
}

webrtc::MediaStreamInterface* MediaStream::GetInterface() {
    return _internalMediaStream;
}

NAN_METHOD(MediaStream::getAudioTracks) {
  TRACE_CALL;

  MediaStream* self = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  webrtc::AudioTrackVector audioTracks = self->_internalMediaStream->GetAudioTracks();

  Local<Array> array = Nan::New<Array>(audioTracks.size());
  int index = 0;
  for (webrtc::AudioTrackVector::iterator track = audioTracks.begin(); track != audioTracks.end(); track++, index++) {
    Local<Value> cargv[1];
    cargv[0] = Nan::New<External>(static_cast<void*>(track->get()));
    array->Set(index, Nan::New(MediaStreamTrack::constructor)->NewInstance(1, cargv));
  }

  TRACE_END;
  info.GetReturnValue().Set(array);
}

NAN_METHOD(MediaStream::getVideoTracks) {
  TRACE_CALL;

  MediaStream* self = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  webrtc::VideoTrackVector videoTracks = self->_internalMediaStream->GetVideoTracks();

  Local<Array> array = Nan::New<Array>(videoTracks.size());
  int index = 0;
  for (webrtc::VideoTrackVector::iterator track = videoTracks.begin(); track != videoTracks.end(); track++, index++) {
    Local<Value> cargv[1];
    cargv[0] = Nan::New<External>(static_cast<void*>(track->get()));
    array->Set(index, Nan::New(MediaStreamTrack::constructor)->NewInstance(1, cargv));
  }

  TRACE_END;
  info.GetReturnValue().Set(array);
}

NAN_METHOD(MediaStream::getTrackById) {
  TRACE_CALL;

  MediaStream* self = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  String::Utf8Value param1(info[0]->ToString());
  std::string _id = std::string(*param1);

  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> audioTrack = self->_internalMediaStream->FindAudioTrack(_id);
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> videoTrack = self->_internalMediaStream->FindVideoTrack(_id);

  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track = audioTrack.get() ? audioTrack : videoTrack;
  webrtc::MediaStreamTrackInterface* msti = track.get();
  msti->AddRef();

  Local<Value> cargv[1];
  cargv[0] = Nan::New<External>(static_cast<void*>(msti));
  Local<Value> mst = Nan::New(MediaStreamTrack::constructor)->NewInstance(1, cargv);

  TRACE_END;
  info.GetReturnValue().Set(mst);
}

NAN_METHOD(MediaStream::addTrack) {
  TRACE_CALL;

  MediaStream* self = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  MediaStreamTrack* _track = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info[0]->ToObject());

  if (_track->GetInterface()->kind() == "audio") {
    self->_internalMediaStream->AddTrack((webrtc::AudioTrackInterface*)_track->GetInterface());
  } else if (_track->GetInterface()->kind() == "video") {
    self->_internalMediaStream->AddTrack((webrtc::VideoTrackInterface*)_track->GetInterface());
  }

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(MediaStream::removeTrack) {
  TRACE_CALL;

  MediaStream* self = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  MediaStreamTrack* _track = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info[0]->ToObject());

  if (_track->GetInterface()->kind() == "audio") {
    self->_internalMediaStream->RemoveTrack((webrtc::AudioTrackInterface*)_track->GetInterface());
  } else if (_track->GetInterface()->kind() == "video") {
    self->_internalMediaStream->RemoveTrack((webrtc::VideoTrackInterface*)_track->GetInterface());
  }

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(MediaStream::clone) {
  TRACE_CALL;

  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_GETTER(MediaStream::GetId) {
  TRACE_CALL;

  MediaStream* self = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());

  std::string label = self->_internalMediaStream->label();

  TRACE_END;
  info.GetReturnValue().Set(Nan::New(label.c_str()).ToLocalChecked());
}

NAN_GETTER(MediaStream::IsInactive) {
  TRACE_CALL;

  MediaStream* self = Nan::ObjectWrap::Unwrap<MediaStream>(info.Holder());
  bool inactive = self->_inactive;

  TRACE_END;
  info.GetReturnValue().Set(Nan::New<Boolean>(inactive));
}

NAN_SETTER(MediaStream::ReadOnly) {
  INFO("MediaStream::ReadOnly");
}


void MediaStream::Init(Handle<Object> exports) {
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(Nan::New("MediaStream").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "getAudioTracks", getAudioTracks);
  Nan::SetPrototypeMethod(tpl, "getVideoTracks", getVideoTracks);
  Nan::SetPrototypeMethod(tpl, "getTrackById", getTrackById);
  Nan::SetPrototypeMethod(tpl, "addTrack", addTrack);
  Nan::SetPrototypeMethod(tpl, "removeTrack", removeTrack);
  Nan::SetPrototypeMethod(tpl, "clone", clone);
  
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("id").ToLocalChecked(), GetId, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("inactive").ToLocalChecked(), IsInactive, ReadOnly);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("MediaStream").ToLocalChecked(), tpl->GetFunction());
}
