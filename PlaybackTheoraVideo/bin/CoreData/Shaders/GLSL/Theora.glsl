#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"

precision highp float;

#if defined(DIFFMAP) || defined(ALPHAMAP)
    varying vec2 vTexCoord;
#endif

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    
    #ifdef DIFFMAP
        vTexCoord = iTexCoord;
    #endif
}

void PS()
{
    vec4 diffColor = cMatDiffColor;
#line 25
    #ifdef DIFFMAP
        vec4 diffInput = vec4(1.0);
        vec3 yuv = texture2D(sDiffMap, vTexCoord).xyz - vec3(0.0, 0.5, 0.5);
         
        yuv.x = 1.1643 * (yuv.x - 0.0625);
        diffInput.x = yuv.x + (1.5958  * yuv.z);
        diffInput.y = yuv.x - (0.39173 * yuv.y) - 0.81290 * yuv.z;
        diffInput.z = yuv.x + (2.017   * yuv.y);
         
        gl_FragColor = diffInput;
    #endif
}
