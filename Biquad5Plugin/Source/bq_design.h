#ifndef BQ_DESIGN_H_
#define BQ_DESIGN_H_

#define BQ_F0_MIN 20
#define BQ_F0_MAX 20000 
#define BQ_Q_MIN 0.1f
#define BQ_Q_MAX 10.0f
#define BQ_GAIN_MIN -30.0f 
#define BQ_GAIN_MAX 20.0f
#define BQ_TYPES_MIN BQ_BYPASS
#define BQ_TYPES_MAX (BQ_TYPES_COUNT-1)

typedef enum
{
   BQ_BYPASS = 0,       ///< Just pass through
   BQ_HISHELF_1ST,      ///< First order Hi-shelving
   BQ_HISHELF_2ND,      ///< Second order Hi-shelving
   BQ_LOSHELF_1ST,      ///< First order Lo-shelving
   BQ_LOSHELF_2ND,      ///< Second order Lo-shelving
   BQ_BPF,              ///< Bandpass
   BQ_PEAK,             ///< Peaking
   BQ_HPF_1ST,          ///< First order highpass
   BQ_HPF_2ND,          ///< Second order highpass
   BQ_LPF_1ST,          ///< First order lowpass, Q-value is not applicable
   BQ_LPF_2ND,          ///< Second order lowpass
   BQ_COFFS = 16,       ///< Coefficients
   BQ_NOTCH = 12,       ///< Notch
   BQ_APF_1ST,          ///< First order all-pass
   BQ_APF_2ND,          ///< Second order all-pass
   BQ_APF = BQ_APF_2ND, ///< Default all-pass (legacy name)
   BQ_TYPES_COUNT
} BiquadType_t;

void bqdesignf(float* b, float* a, float fs, float f0, float gain, float Q, BiquadType_t type);

#endif /*BQ_DESIGN_H_*/
