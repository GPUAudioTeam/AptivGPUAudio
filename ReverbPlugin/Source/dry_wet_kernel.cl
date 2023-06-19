__kernel void dry_wet(float dry, float wet, __global const float *IN, __global float *OUT) {
 
    int i = get_global_id(0);

    OUT[i] = dry * IN[i] + wet * OUT[i];
}