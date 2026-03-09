#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uFont;
uniform vec4 uColor;

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 msdf = texture(uFont, vUV).rgb;
    float sd = median(msdf.r, msdf.g, msdf.b);

    // tweakable
    float screenPxRange = 16.0;
    float alpha = smoothstep(0.5 - screenPxRange / 100.0,
                              0.5 + screenPxRange / 100.0,
                              sd);

    FragColor = vec4(uColor.rgb, uColor.a * alpha);
}
