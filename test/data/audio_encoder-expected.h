#ifndef AUDIO_ENCODER_H_
#define AUDIO_ENCODER_H_

class AudioEncoder {
 public:
  virtual ~AudioEncoder() = default;

 protected:
  AudioEncoder(int sample_rate_hz);
};

class AudioEncoderImpl : public AudioEncoder {
 public:
  AudioEncoderImpl() : AudioEncoder(kSampleRateHz){};
  ~AudioEncoderImpl() override = default;

  AudioEncoderImpl(const AudioEncoderImpl&) = delete;
  AudioEncoderImpl& operator=(const AudioEncoderImpl&) = delete;

 protected:
  int BytesPerSample() const;

 private:
  static const int kSampleRateHz = 8000;
};

#endif  // AUDIO_ENCODER_H_
