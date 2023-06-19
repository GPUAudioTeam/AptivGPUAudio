#include <math.h>
#include <string.h>
#include <complex.h>
#include <stdio.h>
#include "bq_design.h"

#define restrict

#define TWOPI   (6.283185307179586476925286766559)
#define PI      (3.1415926535897932384626433832795f)
#define SQRTTWO (1.4142135623730950488016887242097f)

#define M_MAX_LPF2 5 // number of LPF2 filter coefficients
#define N_MAX_LPF2 9 // number of LPF2 matching points to analog prototype
#define M_MAX_LPF1 3 // number of LPF1 filter coefficients

#define BIQUAD_EPS (2.2204e-16)

void bqdesignf(float* b, float* a, float fs, float f0, float gain, float Q, BiquadType_t type)
{
    float A = 1.0f;
   float alpha;
   float w0;

   // Sanity check of arguments

   if(Q < BQ_Q_MIN)
      Q = BQ_Q_MIN;
   else if(Q > BQ_Q_MAX)
      Q = BQ_Q_MAX;

   if(gain < BQ_GAIN_MIN)
      gain = BQ_GAIN_MIN;
   else if(gain > BQ_GAIN_MAX)
      gain = BQ_GAIN_MAX;

   if(f0 < BQ_F0_MIN)
      f0 = BQ_F0_MIN;
   else if(f0 > BQ_F0_MAX)
      f0 = BQ_F0_MAX;

   w0 = (float)TWOPI*f0/fs;

   if(type == BQ_LOSHELF_2ND || type == BQ_HISHELF_2ND || type == BQ_PEAK)
   {
      A = powf(10.0f, gain*0.025f);
   }
   else if (type == BQ_LOSHELF_1ST || type == BQ_HISHELF_1ST)
   {
      A = powf(10.0f, gain*0.05f);
   }

   if(type == BQ_LOSHELF_2ND || type == BQ_HISHELF_2ND)
   {
      alpha = sinf(w0)/(sqrtf(2.0f*powf(Q, 15.0f/16.0f)));
   }
   else if(type == BQ_LOSHELF_1ST || type == BQ_HISHELF_1ST)
   {
      alpha = tanf(0.5f*w0)/sqrtf(A);
   }
   else if(type == BQ_LPF_1ST || type == BQ_HPF_1ST || type == BQ_APF_1ST)
   {
      alpha = tanf(w0/2.0f);
   }
   else if(type == BQ_HPF_2ND || type == BQ_LPF_2ND || type == BQ_NOTCH)
   {
      alpha = sinf(w0)/(sqrtf(2.0f)*Q);
   }
   else
   {
      if ((type == BQ_PEAK) && (gain <= 0.0f))
      {
         Q = Q * A;
      }
      if ((type == BQ_PEAK) && (gain > 0.0f))
      {
         Q = Q / A;
      }
      alpha = sinf(w0)/(2.0f*Q);
   }

   switch(type)
   {
   case BQ_LPF_2ND:
      a[0] = 1.0f + alpha;
      a[1] = -2.0f*cosf(w0);
      a[2] = 1.0f - alpha;
      b[0] = (1.0f - cosf(w0))*0.5f;
      b[1] = 1.0f - cosf(w0);
      b[2] = (1.0f - cosf(w0))*0.5f;
   break;

   case BQ_LPF_1ST:
      a[0] = alpha + 1.0f;
      a[1] = alpha - 1.0f;
      a[2] = 0.0f;
      b[0] = alpha;
      b[1] = alpha;
      b[2] = 0.0f;
   break;

   case BQ_HPF_2ND:
      a[0] = 1.0f + alpha;
      a[1] = -2.0f*cosf(w0);
      a[2] = 1.0f - alpha;
      b[0] = (1.0f + cosf(w0))*0.5f;
      b[1] = -(1.0f + cosf(w0));
      b[2] = (1.0f + cosf(w0))*0.5f;
   break;

   case BQ_HPF_1ST:
      a[0] = alpha + 1.0f;
      a[1] = alpha - 1.0f;
      a[2] = 0.0f;
      b[0] = 1.0f;
      b[1] = -1.0f;
      b[2] = 0.0f;
   break;

   case BQ_BPF:
      a[0] = 1.0f + alpha;
      a[1] = -2.0f*cosf(w0);
      a[2] = 1.0f - alpha;
      b[0] = sinf(w0)*0.5f;
      b[1] = 0.0f;
      b[2] = -sinf(w0)*0.5f;
   break;

   case BQ_APF_1ST:
      a[0] = alpha + 1.0f;
      a[1] = alpha - 1.0f;
      a[2] = 0.0f;
      b[0] = alpha - 1.0f;
      b[1] = alpha + 1.0f;
      b[2] = 0.0f;
   break;

   case BQ_APF_2ND:
      a[0] = 1.0f + alpha;
      a[1] = -2.0f*cosf(w0);
      a[2] = 1.0f - alpha;
      b[0] = 1.0f - alpha;
      b[1] = -2.0f*cosf(w0);
      b[2] = 1.0f + alpha;
   break;

   case BQ_NOTCH:
      a[0] = 1.0f + alpha;
      a[1] = -2.0f*cosf(w0);
      a[2] = 1.0f - alpha;
      b[0] = 1.0f;
      b[1] = -2.0f*cosf(w0);
      b[2] = 1.0f;
   break;

   case BQ_PEAK:
      a[0] = 1.0f + alpha/A;
      a[1] = -2.0f*cosf(w0);
      a[2] = 1.0f - alpha/A;
      b[0] = 1.0f + alpha*A;
      b[1] = -2.0f*cosf(w0);
      b[2] = 1.0f - alpha*A;
   break;

   case BQ_LOSHELF_1ST:
   {
      a[0] = alpha + 1.0f;
      a[1] = alpha - 1.0f;
      a[2] = 0.0f;
      b[0] = A*alpha + 1.0f;
      b[1] = A*alpha - 1.0f;
      b[2] = 0.0f;
   }
   break;

   case BQ_LOSHELF_2ND:
      a[0] = (A + 1.0f) + (A - 1.0f)*cosf(w0) + 2.0f*sqrtf(A)*alpha;
      a[1] = -2.0f*((A - 1.0f) + (A + 1.0f)*cosf(w0));
      a[2] = (A + 1.0f) + (A - 1.0f)*cosf(w0) - 2.0f*sqrtf(A)*alpha;
      b[0] = A*((A + 1.0f) - (A - 1.0f)*cosf(w0) + 2.0f*sqrtf(A)*alpha);
      b[1] = 2.0f*A*((A - 1.0f) - (A + 1.0f)*cosf(w0));
      b[2] = A*((A + 1.0f) - (A - 1.0f)*cosf(w0) - 2.0f*sqrtf(A)*alpha);
   break;

   case BQ_HISHELF_1ST:
   {
      a[0] = A*alpha + 1.0f;
      a[1] = A*alpha - 1.0f;
      a[2] = 0.0f;
      b[0] = A*alpha + A;
      b[1] = A*alpha - A;
      b[2] = 0.0f;
   }
   break;

   case BQ_HISHELF_2ND:
      a[0] = (A + 1.0f) - (A - 1.0f)*cosf(w0) + 2.0f*sqrtf(A)*alpha;
      a[1] = 2.0f*((A - 1.0f) - (A + 1)*cosf(w0));
      a[2] = (A + 1.0f) - (A - 1)*cosf(w0) - 2.0f*sqrtf(A)*alpha;
      b[0] = A*((A + 1.0f) + (A - 1.0f)*cosf(w0) + 2.0f*sqrtf(A)*alpha);
      b[1] = -2.0f*A*((A - 1.0f) + (A + 1.0f)*cosf(w0));
      b[2] = A*((A + 1.0f) + (A - 1.0f)* cosf(w0) - 2.0f*sqrtf(A)*alpha);
   break;

   case BQ_BYPASS:
   default:
   type = BQ_BYPASS;
      a[0] = 0.0f;
      a[1] = 0.0f;
      a[2] = 0.0f;
      b[0] = 1.0f;
      b[1] = 0.0f;
      b[2] = 0.0f;
   break;
   }

   if(type != BQ_BYPASS)
   {
      float inv_a0 = 1.0f/a[0];
      int i;
      for(i = 0; i < 3; i++)
      {
         a[i] = a[i] * inv_a0;
         b[i] = b[i] * inv_a0;
      }
   }
}
