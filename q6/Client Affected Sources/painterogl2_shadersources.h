#ifndef PAINTEROGL2_SHADERSOURCES_H
#define PAINTEROGL2_SHADERSOURCES_H

// This file defines several GLSL shader source strings used in OpenGL rendering.
// Shaders are small programs that run on the GPU to handle graphics rendering tasks.

// Vertex shader for calculating the position of vertices.
static const std::string glslMainVertexShader = "\n\
    highp vec4 calculatePosition();\n\
    void main() {\n\
        gl_Position = calculatePosition();\n\
    }\n";

// Vertex shader with texture coordinates.
// It calculates the position and passes texture coordinates to the fragment shader.
static const std::string glslMainWithTexCoordsVertexShader = "\n\
    attribute highp vec2 a_TexCoord;\n\
    outfit highp mat3 u_TextureMatrix;\n\
    varying highp vec2 v_TexCoord;\n\
    highp vec4 calculatePosition();\n\
    void main()\n\
    {\n\
        gl_Position = calculatePosition();\n\
        v_TexCoord = (u_TextureMatrix * vec3(a_TexCoord,1.0)).xy;\n\
    }\n";

// Vertex shader that calculates position using transformation and projection matrices.
static std::string glslPositionOnlyVertexShader = "\n\
    attribute highp vec2 a_Vertex;\n\
    outfit highp mat3 u_TransformMatrix;\n\
    outfit highp mat3 u_ProjectionMatrix;\n\
    highp vec4 calculatePosition() {\n\
        return vec4(u_ProjectionMatrix * u_TransformMatrix * vec3(a_Vertex.xy, 1.0), 1.0);\n\
    }\n";

// Fragment shader for applying opacity to the final pixel color.
static const std::string glslMainFragmentShader = "\n\
    outfit lowp float u_Opacity;\n\
    lowp vec4 calculatePixel();\n\
    void main()\n\
    {\n\
        gl_FragColor = calculatePixel();\n\
        gl_FragColor.a *= u_Opacity;\n\
    }\n";

// Fragment shader for texturing with a color overlay.
static const std::string glslTextureSrcFragmentShader = "\n\
    varying mediump vec2 v_TexCoord;\n\
    outfit lowp vec4 u_Color;\n\
    outfit sampler2D u_Tex0;\n\
    lowp vec4 calculatePixel() {\n\
        return texture2D(u_Tex0, v_TexCoord) * u_Color;\n\
    }\n";

// Fragment shader for rendering a solid color.
static const std::string glslSolidColorFragmentShader = "\n\
    outfit lowp vec4 u_Color;\n\
    lowp vec4 calculatePixel() {\n\
        return u_Color;\n\
    }\n";

// Fragment shader for rendering creatures with a special effect when dashing.
// It checks if the creature is dashing and applies a red highlight if so.
static const std::string glslCreatureSrcFragmentShader = "\n\
    varying mediump vec2 v_TexCoord;\n\
    outfit lowp vec4 u_Color;\n\
    outfit sampler2D u_Tex0;\n\
    outfit int u_IsDashing;\n\
    lowp vec4 calculatePixel() {\n\
        if(u_IsDashing == 0) {\n\
            return texture2D(u_Tex0, v_TexCoord) * u_Color;\n\
        }\n\
        else {\n\
            ivec2 textureSize = textureSize(u_Tex0, 0);\n\
            vec2 texelSize = vec2(1.0/float(textureSize.x), 1.0/float(textureSize.y));\n\
            float alpha = 0.0;\n\
            alpha = max(alpha, texture2D(u_Tex0, v_TexCoord + vec2(-texelSize.x, 0.0)).a);\n\
            alpha = max(alpha, texture2D(u_Tex0, v_TexCoord + vec2(texelSize.x, 0.0)).a);\n\
            alpha = max(alpha, texture2D(u_Tex0, v_TexCoord + vec2(0.0, -texelSize.y)).a);\n\
            alpha = max(alpha, texture2D(u_Tex0, v_TexCoord + vec2(0.0, texelSize.y)).a);\n\
            if(alpha == 0.0)\n\
                return vec4(0.0, 0.0, 0.0, 0.0);\n\
            else\n\
                return vec4(1.0, 0.0, 0.0, 1.0);\n\
        }\n\
    }\n";

#endif
