#include <node.h>
#include <v8.h>

#include "talk/app/webrtc/peerconnectioninterface.h"

#include "common.h"
#include "mediastream.h"

using v8::Boolean;
using v8::External;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

static const char *MEDIA_STREAM_NAME = "node-webrtc";
static const char *AUDIO_TRACK_NAME = "node-webrtc-audio";

static rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peerConnectionFactory = webrtc::CreatePeerConnectionFactory();

static rtc::scoped_refptr<webrtc::MediaStreamInterface> getUserMedia(bool audio, bool video) {
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream =
    peerConnectionFactory->CreateLocalMediaStream(MEDIA_STREAM_NAME);
  if (audio) {
    rtc::scoped_refptr<webrtc::AudioTrackInterface> audioTrack(
      peerConnectionFactory->CreateAudioTrack(AUDIO_TRACK_NAME,
        peerConnectionFactory->CreateAudioSource(NULL)));
    stream->AddTrack(audioTrack);
  }
  if (video) {
    // TODO(mroberts): Add video support.
  }
  return stream;
}

NAN_METHOD(GetUserMedia) {
  TRACE_CALL;

  Local<Object> options = Local<Object>::Cast(info[0]);
  /*Local<Boolean> audio = options->Get(String::NewSymbol("audio"))->ToBoolean();
  Local<Boolean> video = options->Get(String::NewSymbol("video"))->ToBoolean();*/

  /*rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = getUserMedia(audio->Value(), video->Value());*/
  rtc::scoped_refptr<webrtc::MediaStreamInterface> stream = getUserMedia(true, false);

  Local<Value> cargv[1];
  cargv[0] = Nan::New<External>(static_cast<void*>(stream));

  Local<Value> wrapped = Nan::New(MediaStream::constructor)->NewInstance(1, cargv);

  TRACE_END;
  info.GetReturnValue().Set(wrapped);
}
