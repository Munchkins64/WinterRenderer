#version 450

layout(location = 0) out vec2 outUV;

void main() {
    // 5. Converts vertex ID to fullscreen triangle coordinate mathematically
    int vid  = int(gl_VertexIndex);
    outUV    = vec2(float((vid << 1) & 2), float(vid & 2));
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}