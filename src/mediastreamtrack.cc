#include <node_buffer.h>

#include "talk/app/webrtc/jsep.h"
#include "webrtc/system_wrappers/interface/ref_count.h"

#include "common.h"
#include "mediastreamtrack.h"

using namespace node;
using v8::Boolean;
using v8::External;
using v8::Function;
using v8::Handle;
using v8::Local;
using v8::Object;
using v8::Number;
using v8::Value;

Nan::Persistent<Function> MediaStreamTrack::constructor;

MediaStreamTrack::MediaStreamTrack(webrtc::MediaStreamTrackInterface* msti)
: _internalMediaStreamTrack(msti)
{
  msti->Release();
  _muted = false;
  _live = _internalMediaStreamTrack->state() == webrtc::MediaStreamTrackInterface::kLive;
  uv_mutex_init(&lock);
  uv_async_init(uv_default_loop(), &async, Run);

  async.data = this;
}

MediaStreamTrack::~MediaStreamTrack()
{

}

NAN_METHOD(MediaStreamTrack::New) {
  TRACE_CALL;
  
  if(!info.IsConstructCall()) {
    return Nan::ThrowTypeError("Use the new operator to construct the MediaStreamTrack.");
  }

  Local<External> _msti = Local<External>::Cast(info[0]);
  webrtc::MediaStreamTrackInterface* msti = static_cast<webrtc::MediaStreamTrackInterface*>(_msti->Value());

  MediaStreamTrack* obj = new MediaStreamTrack(msti);
  msti->RegisterObserver(obj);
  obj->Wrap(info.This());

  TRACE_END;
  info.GetReturnValue().Set(info.This());
}

void MediaStreamTrack::QueueEvent(AsyncEventType type, void* data)
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

void MediaStreamTrack::Run(uv_async_t* handle, int status)
{
  TRACE_CALL;
    MediaStreamTrack* self = static_cast<MediaStreamTrack*>(handle->data);
  Local<Object> mst = self->handle();

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
    if(MediaStreamTrack::CHANGE & evt.type)
    {
      // find current state and send appropriate events
      bool live = self->_internalMediaStreamTrack->state() == webrtc::MediaStreamTrackInterface::kLive;
      if (self->_live != live)
      {
        if (!live)
          evt.type = MediaStreamTrack::ENDED;
        else
          evt.type = MediaStreamTrack::STARTED;
        self->_live = live;
      }
    }
    if(MediaStreamTrack::MUTE & evt.type)
    {
      Local<Function> callback = Local<Function>::Cast(mst->Get(Nan::New("onmute").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[0];
        Nan::MakeCallback(mst, callback, 0, argv);
      }
    } else if(MediaStreamTrack::UNMUTE & evt.type)
    {
      Local<Function> callback = Local<Function>::Cast(mst->Get(Nan::New("onunmute").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[0];
        Nan::MakeCallback(mst, callback, 0, argv);
      }
    }
    if(MediaStreamTrack::STARTED & evt.type)
    {
      Local<Function> callback = Local<Function>::Cast(mst->Get(Nan::New("onstarted").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[0];
        Nan::MakeCallback(mst, callback, 0, argv);
      }
    }
    if(MediaStreamTrack::ENDED & evt.type)
    {
      Local<Function> callback = Local<Function>::Cast(mst->Get(Nan::New("onended").ToLocalChecked()));
      if(!callback.IsEmpty())
      {
        Local<Value> argv[0];
        Nan::MakeCallback(mst, callback, 0, argv);
      }
    }

  }
  scope.Close(Undefined());
  TRACE_END;
}

void MediaStreamTrack::OnChanged()
{
  TRACE_CALL;
  QueueEvent(MediaStreamTrack::CHANGE, static_cast<void*>(NULL));
  TRACE_END;
}

webrtc::MediaStreamTrackInterface* MediaStreamTrack::GetInterface() {
    return _internalMediaStreamTrack;
}

NAN_METHOD(MediaStreamTrack::clone) {
  TRACE_CALL;
    // todo: implement
  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_METHOD(MediaStreamTrack::stop) {
  TRACE_CALL;
    // todo: implement
  TRACE_END;
  info.GetReturnValue().Set(Nan::Undefined());
}

NAN_GETTER(MediaStreamTrack::GetId) {
  TRACE_CALL;
  
  MediaStreamTrack* self = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info.Holder());
  std::string id = self->_internalMediaStreamTrack->id();

  TRACE_END;
  info.GetReturnValue().Set(Nan::New(id.c_str()).ToLocalChecked());
}

NAN_GETTER(MediaStreamTrack::GetLabel) {
  TRACE_CALL;
  
  MediaStreamTrack* self = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info.Holder());
  std::string label = self->_internalMediaStreamTrack->id();

  TRACE_END;
  info.GetReturnValue().Set(Nan::New(label.c_str()).ToLocalChecked());
}

NAN_GETTER(MediaStreamTrack::GetKind) {
  TRACE_CALL;
  
  MediaStreamTrack* self = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info.Holder());
  std::string kind = self->_internalMediaStreamTrack->kind();

  TRACE_END;
  info.GetReturnValue().Set(Nan::New(kind.c_str()).ToLocalChecked());
}

NAN_GETTER(MediaStreamTrack::GetEnabled) {
  TRACE_CALL;
  
  MediaStreamTrack* self = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info.Holder());
  bool enabled = self->_internalMediaStreamTrack->enabled();

  TRACE_END;
  info.GetReturnValue().Set(Nan::New<Boolean>(enabled))
}

NAN_GETTER(MediaStreamTrack::GetMuted) {
  TRACE_CALL;
    TRACE_END;
  info.GetReturnValue().Set(Nan::False())
}

NAN_GETTER(MediaStreamTrack::GetReadOnly) {
  TRACE_CALL;
    TRACE_END;
  info.GetReturnValue().Set(Nan::False())
}

NAN_GETTER(MediaStreamTrack::GetRemote) {
  TRACE_CALL;
    TRACE_END;
  info.GetReturnValue().Set(Nan::False())
}

NAN_GETTER(MediaStreamTrack::GetReadyState) {
  TRACE_CALL;
  
  MediaStreamTrack* self = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info.Holder());

  webrtc::MediaStreamTrackInterface::TrackState state = self->_internalMediaStreamTrack->state();

  TRACE_END;
  info.GetReturnValue().Set(Nan::New<Number>(static_cast<uint32_t>(state)))
}

NAN_SETTER(MediaStreamTrack::SetEnabled) {
  TRACE_CALL;
  
  MediaStreamTrack* self = Nan::ObjectWrap::Unwrap<MediaStreamTrack>(info.Holder());
  self->_internalMediaStreamTrack->set_enabled(value->ToBoolean()->Value());

  TRACE_END;
}

NAN_SETTER(MediaStreamTrack::ReadOnly) {
  INFO("MediaStreamTrack::ReadOnly");
}

void MediaStreamTrack::Init( Handle<Object> exports ) {
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(Nan::New("MediaStreamTrack").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "clone", clone);
  Nan::SetPrototypeMethod(tpl, "stop", stop);

  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("id").ToLocalChecked(), GetId, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("kind").ToLocalChecked(), GetKind, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("label").ToLocalChecked(), GetLabel, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("enabled").ToLocalChecked(), GetEnabled, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("muted").ToLocalChecked(), GetMuted, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("_readonly").ToLocalChecked(), GetReadOnly, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("remote").ToLocalChecked(), GetRemote, ReadOnly);
  Nan::SetAccessor(tpl->InstanceTemplate(), Nan::New("readyState").ToLocalChecked(), GetReadyState, ReadOnly);

  constructor.Reset(tpl->GetFunction());
  exports->Set(Nan::New("MediaStreamTrack").ToLocalChecked(), tpl->GetFunction() );
}
