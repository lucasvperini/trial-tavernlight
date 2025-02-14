#ifndef PAINTER_H
#define PAINTER_H

#include <framework/graphics/declarations.h>
#include <framework/graphics/coordsbuffer.h>
#include <framework/graphics/paintershaderprogram.h>
#include <framework/graphics/texture.h>
#include <framework/graphics/brushconfiguration.h>

class Painter
{
public:
    enum BlendEquation {
        BlendEquation_Add,
        BlendEquation_Max
    };
    enum CompositionMode {
        CompositionMode_Normal,
        CompositionMode_Multiply,
        CompositionMode_Add,
        CompositionMode_Replace,
        CompositionMode_DestBlending,
        CompositionMode_Light
    };
    enum DrawMode {
        Triangles = GL_TRIANGLES,
        TriangleStrip = GL_TRIANGLE_STRIP
    };
    enum PaintType {
        PaintType_Textured,
        PaintType_SolidColor,
        PaintType_Creature,
    };

    Painter();
    virtual ~Painter() { }

    virtual void bind() { }
    virtual void unbind() { }

    virtual void saveState() = 0;
    virtual void saveAndResetState() = 0;
    virtual void restoreSavedState() = 0;

    virtual void clear(const Color& color) = 0;

    virtual void drawCoords(CoordsBuffer& coordsBuffer, DrawMode drawMode = Triangles) = 0;
    virtual void drawFillCoords(CoordsBuffer& coordsBuffer) = 0;
    virtual void drawTextureCoords(CoordsBuffer& coordsBuffer, const TexturePtr& texture) = 0;
    virtual void drawTexturedRect(const Rect& dest, const TexturePtr& texture, const Rect& src) = 0;
    void drawTexturedRect(const Rect& dest, const TexturePtr& texture) { drawTexturedRect(dest, texture, Rect(Point(0,0), texture->getSize())); }
    virtual void drawUpsideDownTexturedRect(const Rect& dest, const TexturePtr& texture, const Rect& src) = 0;
    virtual void drawRepeatedTexturedRect(const Rect& dest, const TexturePtr& texture, const Rect& src) = 0;
    virtual void drawFilledRect(const Rect& dest) = 0;
    virtual void drawFilledTriangle(const Point& a, const Point& b, const Point& c) = 0;
    virtual void drawBoundingRect(const Rect& dest, int innerLineWidth = 1) = 0;

    virtual void setTexture(Texture *texture) = 0;
    virtual void setClipRect(const Rect& clipRect) = 0;
    virtual void setColor(const Color& color) { m_color = color; }
    virtual void setAlphaWriting(bool enable) = 0;
    virtual void setBlendEquation(BlendEquation blendEquation) = 0;
    virtual void setShaderProgram(PainterShaderProgram *shaderProgram) { m_shaderProgram = shaderProgram; }
    void setShaderProgram(const PainterShaderProgramPtr& shaderProgram) { setShaderProgram(shaderProgram.get()); }

    
    virtual void applyPaintType(PaintType paintType) { }
    virtual void setBrushConfiguration(const BrushConfiguration& brushConfiguration) { }
    virtual void flushBrushConfigurations(PaintType paintType) { };

    virtual void scale(float x, float y) = 0;
    void scale(float factor) { scale(factor, factor); }
    virtual void translate(float x, float y) = 0;
    void translate(const Point& p) { translate(p.x, p.y); }
    virtual void rotate(float angle) = 0;
    virtual void rotate(float x, float y, float angle) = 0;
    void rotate(const Point& p, float angle) { rotate(p.x, p.y, angle); }

    virtual void setOpacity(float opacity) { m_opacity = opacity; }
    virtual void setResolution(const Size& resolution) { m_resolution = resolution; }

    Size getResolution() { return m_resolution; }
    Color getColor() { return m_color; }
    float getOpacity() { return m_opacity; }
    Rect getClipRect() { return m_clipRect; }
    CompositionMode getCompositionMode() { return m_compositionMode; }

    virtual void setCompositionMode(CompositionMode compositionMode) = 0;

    virtual void pushTransformMatrix() = 0;
    virtual void popTransformMatrix() = 0;

    void resetClipRect() { setClipRect(Rect()); }
    void resetOpacity() { setOpacity(1.0f); }
    void resetCompositionMode() { setCompositionMode(CompositionMode_Normal); }
    void resetColor() { setColor(Color::white); }
    void resetShaderProgram() { setShaderProgram(nullptr); }

    virtual bool hasShaders() = 0;

protected:
    PainterShaderProgram *m_shaderProgram;
    CompositionMode m_compositionMode;
    Color m_color;
    Size m_resolution;
    float m_opacity;
    Rect m_clipRect;

    PaintType m_paintType;
    std::vector<BrushConfiguration> m_brushConfigurationVector;
};

extern Painter *g_painter;

#endif
