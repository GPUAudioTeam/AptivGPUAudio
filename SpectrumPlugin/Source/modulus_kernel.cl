__kernel void modulus(__global const float *IN, __global float *OUT)
{
    int i = get_global_id(0);
    float re = IN[2*i];
    float im = IN[2*i+1];
    OUT[i] = sqrt( re*re + im*im );
}
