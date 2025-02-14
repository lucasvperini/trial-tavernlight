#include "painterogl2.h"
#include "painterogl2_shadersources.h"
#include <framework/platform/platformwindow.h>

// Global pointer to the PainterOGL2 instance
PainterOGL2 *g_painterOGL2 = nullptr;

// Constructor for the PainterOGL2 class
PainterOGL2::PainterOGL2()
{
    // Resets the state of the painter
    resetState();
    
    // Initialize shader programs to nullptr
    m_drawProgram = nullptr;
    
    // Create shared pointers for different shader programs
    m_drawTexturedProgram = std::make_shared<PainterShaderProgram>();
    m_drawSolidColorProgram = std::make_shared<PainterShaderProgram>();
    m_drawCreatureProgram = std::make_shared<PainterShaderProgram>();
    
    // Ensure that all shader programs are successfully created
    assert(m_drawTexturedProgram && m_drawSolidColorProgram && m_drawCreatureProgram);
    
    // Add vertex and fragment shaders to the textured program and link them
    m_drawTexturedProgram->addShaderFromSourceCode(Shader::Vertex, glslMainWithTexCoordsVertexShader + glslPositionOnlyVertexShader);
    m_drawTexturedProgram->addShaderFromSourceCode(Shader::Fragment, glslMainFragmentShader + glslTextureSrcFragmentShader);
    m_drawTexturedProgram->link();
    
    // Add vertex and fragment shaders to the solid color program and link them
    m_drawSolidColorProgram->addShaderFromSourceCode(Shader::Vertex, glslMainVertexShader + glslPositionOnlyVertexShader);
    m_drawSolidColorProgram->addShaderFromSourceCode(Shader::Fragment, glslMainFragmentShader + glslSolidColorFragmentShader);
    m_drawSolidColorProgram->link();
    
    // Add vertex and fragment shaders to the creature program and link them
    m_drawCreatureProgram->addShaderFromSourceCode(Shader::Vertex, glslMainWithTexCoordsVertexShader + glslPositionOnlyVertexShader);
    m_drawCreatureProgram->addShaderFromSourceCode(Shader::Fragment, glslMainFragmentShader + glslCreatureSrcFragmentShader);
    m_drawCreatureProgram->link();
    
    // Bind the creature program and set a outfit value
    m_drawCreatureProgram->bind();
    m_drawCreatureProgram->setOutfitValue("u_IsDashing", 0);
    PainterShaderProgram::release();
}

// Binds the painter and enables necessary attribute arrays
void PainterOGL2::bind()
{
    PainterOGL::bind();
    PainterShaderProgram::enableAttributeArray(PainterShaderProgram::VERTEX_ATTR);
    PainterShaderProgram::enableAttributeArray(PainterShaderProgram::TEXCOORD_ATTR);
}

// Unbinds the painter and disables attribute arrays
void PainterOGL2::unbind()
{
    PainterShaderProgram::disableAttributeArray(PainterShaderProgram::VERTEX_ATTR);
    PainterShaderProgram::disableAttributeArray(PainterShaderProgram::TEXCOORD_ATTR);
    PainterShaderProgram::release();
}

// Draws coordinates using the specified draw mode
void PainterOGL2::drawCoords(CoordsBuffer& coordsBuffer, DrawMode drawMode)
{
    // Check if there are vertices to draw and if the texture is valid
    if(coordsBuffer.getVertexCount() == 0 || (coordsBuffer.getTextureCoordCount() > 0 && m_texture && m_texture->isEmpty()))
        return;
    
    // Bind the current drawing program and set various matrices and properties
    m_drawProgram->bind();
    m_drawProgram->setTransformMatrix(m_transformMatrix);
    m_drawProgram->setProjectionMatrix(m_projectionMatrix);
    m_drawProgram->setOpacity(m_opacity);
    m_drawProgram->setColor(m_color);
    m_drawProgram->setResolution(m_resolution);
    m_drawProgram->updateTime();
    
    // Determine if the drawing is textured
    bool textured = coordsBuffer.getTextureCoordCount() > 0 && m_texture;
    if(textured) {
        m_drawProgram->setTextureMatrix(m_textureMatrix);
        m_drawProgram->bindMultiTextures();
    }
    
    // Update caches and determine if hardware caching is used
    coordsBuffer.updateCaches();
    bool hardwareCached = coordsBuffer.isHardwareCached();
    
    // Set attribute arrays for texture coordinates if textured
    if(textured) {
        m_drawProgram->setAttributeArray(PainterShaderProgram::TEXCOORD_ATTR, hardwareCached ? nullptr : coordsBuffer.getTextureCoordArray(), 2);
    } else {
        PainterShaderProgram::disableAttributeArray(PainterShaderProgram::TEXCOORD_ATTR);
    }
    
    // Set attribute arrays for vertex coordinates
    m_drawProgram->setAttributeArray(PainterShaderProgram::VERTEX_ATTR, hardwareCached ? nullptr : coordsBuffer.getVertexArray(), 2);
    
    // Draw the arrays using the specified draw mode
    glDrawArrays(drawMode == Triangles ? GL_TRIANGLES : GL_TRIANGLE_STRIP, 0, coordsBuffer.getVertexCount());
    
    // Re-enable texture coordinate attribute array if not textured
    if(!textured)
        PainterShaderProgram::enableAttributeArray(PainterShaderProgram::TEXCOORD_ATTR);
}

// Applies the paint type by setting the appropriate shader program
void PainterOGL2::applyPaintType(PaintType paintType)
{
    switch(paintType) {
        case PaintType_Textured:
            setShaderProgram(m_drawTexturedProgram.get());
            break;
        case PaintType_SolidColor:
            setShaderProgram(m_drawSolidColorProgram.get());
            break;
        case PaintType_Creature:
            setShaderProgram(m_drawCreatureProgram.get());
            break;
    }
}

// Flushes brush configurations by setting outfit values in the shader program
void PainterOGL2::flushBrushConfigurations(PaintType paintType)
{
    PainterShaderProgram* shaderProgram = nullptr;
    
    // Select the appropriate shader program based on the paint type
    switch(paintType) {
        case PaintType_Textured:
            shaderProgram = m_drawTexturedProgram.get();
            break;
        case PaintType_SolidColor:
            shaderProgram = m_drawSolidColorProgram.get();
            break;
        case PaintType_Creature:
            shaderProgram = m_drawCreatureProgram.get();
            break;
    }
    
    // If no shader program is selected, clear the brush configuration vector
    if(!shaderProgram) {
        m_brushConfigurationVector.clear();
        return;
    }
    
    // Bind the shader program and set outfit values based on brush configurations
    shaderProgram->bind();
    for(auto& config : m_brushConfigurationVector) {
        switch(config.getType()) {
            case BrushConfiguration::Type_Int32:
                shaderProgram->setOutfitValue(config.getLocation(), config.getInt32Value());
                break;
            case BrushConfiguration::Type_Float:
                shaderProgram->setOutfitValue(config.getLocation(), config.getFloatValue());
                break;
            case BrushConfiguration::Type_Vector2:
                {
                    PointF value = config.getVector2Value();
                    shaderProgram->setOutfitValue(config.getLocation(), value.x, value.y);
                }
                break;
            case BrushConfiguration::Type_Color:
                shaderProgram->setOutfitValue(config.getLocation(), config.getColorValue());
                break;
        }
    }
    
    // Clear the brush configuration vector after applying configurations
    m_brushConfigurationVector.clear();
}