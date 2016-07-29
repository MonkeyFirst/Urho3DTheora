#include "Uniforms.glsl"
#include "Samplers.glsl"
#include "Transform.glsl"

varying vec2 vTexCoord;

void VS()
{
    mat4 modelMatrix = iModelMatrix;
    vec3 worldPos = GetWorldPos(modelMatrix);
    gl_Position = GetClipPos(worldPos);
    
    vTexCoord = iTexCoord;

}
#line 17
void PS()
{

    float y = texture2D(sDiffMap, vTexCoord.xy).r;
    float u = texture2D(sSpecMap, vTexCoord.xy).r - 0.5;
    float v = texture2D(sNormalMap, vTexCoord.xy).r - 0.5;

    float r = y + 1.13983*v;
    float g = y - 0.39465*u-0.58060*v;
    float b = y + 2.03211*u;

    gl_FragColor = vec4(r,g,b,1);
    
    //gl_FragColor = vec4(y,y,y,1); // BW video
    
}
