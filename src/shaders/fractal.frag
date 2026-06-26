#version 450
#extension GL_ARB_gpu_shader_fp64 : enable

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PC {
    vec2   resolution;
    float  time;
    int    maxIterations;
    dvec2  centerX;
    dvec2  centerY;
    double zoom;
    int    fractalType;
    float  _pad1;
    vec2   juliaC;
    float  colorOffset;
} pc;

// 1. Double-double representation using two doubles (high and low parts)
struct ddreal {
    double x;
    double y;
};

ddreal dd_set(double d) {
    return ddreal(d, 0.0);
}

// 2. High-precision double-double addition using Knuth's two-sum algorithm
ddreal dd_add(ddreal a, ddreal b) {
    double s = a.x + b.x;
    double v = s - a.x;
    double e = (a.x - (s - v)) + (b.x - v) + a.y + b.y;
    double zh = s + e;
    double zl = e - (zh - s);
    return ddreal(zh, zl);
}

ddreal dd_sub(ddreal a, ddreal b) {
    return dd_add(a, ddreal(-b.x, -b.y));
}

// 3. Fast double-double multiplication exploiting hardware fused multiply-add (fma)
ddreal dd_mul(ddreal a, ddreal b) {
    double q = a.x * b.x;
    double e = fma(a.x, b.x, -q) + a.x * b.y + a.y * b.x;
    double zh = q + e;
    double zl = e - (zh - q);
    return ddreal(zh, zl);
}

vec3 palette(float t) {
    vec3 a = vec3(0.5);
    vec3 b = vec3(0.5);
    vec3 c = vec3(1.0, 1.0, 0.5);
    vec3 d = vec3(0.80, 0.90, 0.30);
    return a + b * cos(6.28318 * (c * t + d));
}

void main() {
    double aspect = double(pc.resolution.x) / double(pc.resolution.y);

    int iter = pc.maxIterations;
    double finalR2 = 0.0;

    if (pc.zoom < 1e13) {
        // 4. Fallback standard double-precision loop for high performance at lower zoom levels
        double normX = (double(inUV.x) - 0.5) * aspect * (2.5 / pc.zoom);
        double normY = (double(inUV.y) - 0.5) * (2.5 / pc.zoom);
        double c_re = normX + (pc.centerX.x + pc.centerX.y);
        double c_it = normY + (pc.centerY.x + pc.centerY.y);

        double z_re = (pc.fractalType == 1) ? c_re : 0.0;
        double z_im = (pc.fractalType == 1) ? c_it : 0.0;
        double s_re = (pc.fractalType == 1) ? double(pc.juliaC.x) : c_re;
        double s_im = (pc.fractalType == 1) ? double(pc.juliaC.y) : c_it;

        for (int i = 0; i < pc.maxIterations; i++) {
            double r2 = z_re*z_re + z_im*z_im;
            if (r2 > 4.0) {
                iter = i;
                finalR2 = r2;
                break;
            }
            double next_re;
            if (pc.fractalType == 2) {
                next_re = abs(z_re)*abs(z_re) - abs(z_im)*abs(z_im) + s_re;
                z_im = 2.0*abs(z_re)*abs(z_im) + s_im;
            } else if (pc.fractalType == 3) {
                next_re = z_re*z_re - z_im*z_im + s_re;
                z_im = -2.0*z_re*z_im + s_im;
            } else {
                next_re = z_re*z_re - z_im*z_im + s_re;
                z_im = 2.0*z_re*z_im + s_im;
            }
            z_re = next_re;
        }
    } else {
        // 5. High-precision double-double loop activated only for extreme zoom levels
        ddreal normX = dd_mul(dd_set((double(inUV.x) - 0.5) * aspect), dd_set(2.5 / pc.zoom));
        ddreal normY = dd_mul(dd_set(double(inUV.y) - 0.5), dd_set(2.5 / pc.zoom));

        ddreal c_re = dd_add(normX, ddreal(pc.centerX.x, pc.centerX.y));
        ddreal c_it = dd_add(normY, ddreal(pc.centerY.x, pc.centerY.y));

        ddreal z_re = (pc.fractalType == 1) ? c_re : dd_set(0.0);
        ddreal z_im = (pc.fractalType == 1) ? c_it : dd_set(0.0);
        ddreal s_re = (pc.fractalType == 1) ? dd_set(double(pc.juliaC.x)) : c_re;
        ddreal s_im = (pc.fractalType == 1) ? dd_set(double(pc.juliaC.y)) : c_it;

        for (int i = 0; i < pc.maxIterations; i++) {
            ddreal r2 = dd_add(dd_mul(z_re, z_re), dd_mul(z_im, z_im));
            if (r2.x > 4.0) {
                iter = i;
                finalR2 = r2.x;
                break;
            }
            ddreal next_re;
            if (pc.fractalType == 2) {
                ddreal abs_re = ddreal(abs(z_re.x), abs(z_re.y));
                ddreal abs_im = ddreal(abs(z_im.x), abs(z_im.y));
                next_re = dd_add(dd_sub(dd_mul(abs_re, abs_re), dd_mul(abs_im, abs_im)), s_re);
                z_im = dd_add(dd_mul(dd_set(2.0), dd_mul(abs_re, abs_im)), s_im);
            } else if (pc.fractalType == 3) {
                next_re = dd_add(dd_sub(dd_mul(z_re, z_re), dd_mul(z_im, z_im)), s_re);
                z_im = dd_add(dd_mul(dd_set(-2.0), dd_mul(z_re, z_im)), s_im);
            } else {
                next_re = dd_add(dd_sub(dd_mul(z_re, z_re), dd_mul(z_im, z_im)), s_re);
                z_im = dd_add(dd_mul(dd_set(2.0), dd_mul(z_re, z_im)), s_im);
            }
            z_re = next_re;
        }
    }

    if (iter == pc.maxIterations) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        // 6. Smooth logarithmic shading calculation to prevent color banding
        float s = float(iter) - log2(max(1.0f, log2(float(finalR2))));
        outColor = vec4(palette(fract(s / float(pc.maxIterations) * 3.0 + pc.colorOffset)), 1.0);
    }
}