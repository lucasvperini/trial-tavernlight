#ifndef PAINTEROGL2_H
#define PAINTEROGL2_H

#define PAINTER_OGL2

#include "painterogl.h"

/**
 * Painter using OpenGL 2.0 programmable rendering pipeline,
 * compatible with OpenGL ES 2.0. Only recent cards support
 * this painter engine.
 */
class PainterOGL2 : public PainterOGL
{
public:
    PainterOGL2();

    void bind();
    void unbind();

    void drawCoords(CoordsBuffer& coordsBuffer, DrawMode drawMode = Triangles);
    void drawFillCoords(CoordsBuffer& coordsBuffer);
    void drawTextureCoords(CoordsBuffer& coordsBuffer, const TexturePtr& texture);
    void drawTexturedRect(const Rect& dest, const TexturePtr& texture, const Rect& src);
    void drawUpsideDownTexturedRect(const Rect& dest, const TexturePtr& texture, const Rect& src);
    void drawRepeatedTexturedRect(const Rect& dest, const TexturePtr& texture, const Rect& src);
    void drawFilledRect(const Rect& dest);
    void drawFilledTriangle(const Point& a, const Point& b, const Point& c);
    void drawBoundingRect(const Rect& dest, int innerLineWidth = 1);

    void setDrawProgram(PainterShaderProgram *drawProgram) { m_drawProgram = drawProgram; }

    void applyPaintType(PaintType paintType);
    void setBrushConfiguration(const BrushConfiguration& brushConfiguration);
    void flushBrushConfigurations(PaintType paintType);

    bool hasShaders() { return true; }

private:
    PainterShaderProgram *m_drawProgram;

    PainterShaderProgramPtr m_drawTexturedProgram;
    PainterShaderProgramPtr m_drawSolidColorProgram;

    PainterShaderProgramPtr m_drawCreatureProgram;
};

extern PainterOGL2 *g_painterOGL2;

#endif
