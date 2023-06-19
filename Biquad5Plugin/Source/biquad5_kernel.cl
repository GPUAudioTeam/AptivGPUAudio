__kernel void biquad5(__global const float* invec, __global float* outvec, __global float* delay, __global const float* coeffsA, __global const float* coeffsB, int nbOfSamples)
{
    int ch = get_global_id(0);

    const int nbOfBiquads = 5;
    const int nbOfDelays = 2;
    const int nbOfCoeffs = 3;

    float state0A, state1A;
    float state0B, state1B;
    float state0C, state1C;
    float state0D, state1D;
    float state0E, state1E;

    float a1A, a2A, b0A, b1A, b2A;
    float a1B, a2B, b0B, b1B, b2B;
    float a1C, a2C, b0C, b1C, b2C;
    float a1D, a2D, b0D, b1D, b2D;
    float a1E, a2E, b0E, b1E, b2E;

    float tmpIn, tmpOut;
    __global const float * inA = &invec[ch*nbOfSamples];
    __global float * outA = &outvec[ch*nbOfSamples];
    int offset = ch * nbOfBiquads;

    __global float * delaysA = &delay[offset*nbOfDelays + 0 * nbOfDelays];
    __global float * delaysB = &delay[offset*nbOfDelays + 1 * nbOfDelays];
    __global float * delaysC = &delay[offset*nbOfDelays + 2 * nbOfDelays];
    __global float * delaysD = &delay[offset*nbOfDelays + 3 * nbOfDelays];
    __global float * delaysE = &delay[offset*nbOfDelays + 4 * nbOfDelays];

    __global const float * asA = &coeffsA[offset*nbOfCoeffs + 0 * nbOfCoeffs];
    __global const float * asB = &coeffsA[offset*nbOfCoeffs + 1 * nbOfCoeffs];
    __global const float * asC = &coeffsA[offset*nbOfCoeffs + 2 * nbOfCoeffs];
    __global const float * asD = &coeffsA[offset*nbOfCoeffs + 3 * nbOfCoeffs];
    __global const float * asE = &coeffsA[offset*nbOfCoeffs + 4 * nbOfCoeffs];

    __global const float * bsA = &coeffsB[offset*nbOfCoeffs + 0 * nbOfCoeffs];
    __global const float * bsB = &coeffsB[offset*nbOfCoeffs + 1 * nbOfCoeffs];
    __global const float * bsC = &coeffsB[offset*nbOfCoeffs + 2 * nbOfCoeffs];
    __global const float * bsD = &coeffsB[offset*nbOfCoeffs + 3 * nbOfCoeffs];
    __global const float * bsE = &coeffsB[offset*nbOfCoeffs + 4 * nbOfCoeffs];

    a1A = asA[1], a2A = asA[2], b0A = bsA[0], b1A = bsA[1], b2A = bsA[2];
    a1B = asB[1], a2B = asB[2], b0B = bsB[0], b1B = bsB[1], b2B = bsB[2];
    a1C = asC[1], a2C = asC[2], b0C = bsC[0], b1C = bsC[1], b2C = bsC[2];
    a1D = asD[1], a2D = asD[2], b0D = bsD[0], b1D = bsD[1], b2D = bsD[2];
    a1E = asE[1], a2E = asE[2], b0E = bsE[0], b1E = bsE[1], b2E = bsE[2];

    state0A = delaysA[0], state1A = delaysA[1];
    state0B = delaysB[0], state1B = delaysB[1];
    state0C = delaysC[0], state1C = delaysC[1];
    state0D = delaysD[0], state1D = delaysD[1];
    state0E = delaysE[0], state1E = delaysE[1];

    for(int i = 0; i < nbOfSamples; i++)
    {
        tmpIn = inA[i];
        tmpOut = b0A * tmpIn + state0A;
        state0A = b1A * tmpIn - a1A * tmpOut + state1A;
        state1A = b2A * tmpIn - a2A * tmpOut;

        tmpIn = tmpOut;
        tmpOut = b0B * tmpIn + state0B;
        state0B = b1B * tmpIn - a1B * tmpOut + state1B;
        state1B = b2B * tmpIn - a2B * tmpOut;

        tmpIn = tmpOut;
        tmpOut = b0C * tmpIn + state0C;
        state0C = b1C * tmpIn - a1C * tmpOut + state1C;
        state1C = b2C * tmpIn - a2C * tmpOut;

        tmpIn = tmpOut;
        tmpOut = b0D * tmpIn + state0D;
        state0D = b1D * tmpIn - a1D * tmpOut + state1D;
        state1D = b2D * tmpIn - a2D * tmpOut;

        tmpIn = tmpOut;
        tmpOut = b0E * tmpIn + state0E;
        state0E = b1E * tmpIn - a1E * tmpOut + state1E;
        state1E = b2E * tmpIn - a2E * tmpOut;

        outA[i] = tmpOut;
    }

    delaysA[0] = state0A, delaysA[1] = state1A;
    delaysB[0] = state0B, delaysB[1] = state1B;
    delaysC[0] = state0C, delaysC[1] = state1C;
    delaysD[0] = state0D, delaysD[1] = state1D;
    delaysE[0] = state0E, delaysE[1] = state1E;
}
