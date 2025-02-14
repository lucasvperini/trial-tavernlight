#ifndef MAPVIEW_H
#define MAPVIEW_H

#include "declarations.h"
#include <framework/graphics/paintershaderprogram.h>
#include <framework/graphics/declarations.h>
#include <framework/luaengine/luaobject.h>
#include <framework/core/declarations.h>
#include "lightview.h"
#include "localeffect.h"


class MapView : public LuaObject
{
public:
    enum ViewMode {
        NEAR_VIEW,
        MID_VIEW,
        FAR_VIEW,
        HUGE_VIEW
    };

    MapView();
    ~MapView();
    void draw(const Rect& rect);

private:
    void updateGeometry(const Size& visibleDimension, const Size& optimizedSize);
    void updateVisibleTilesCache(int start = 0);
    void requestVisibleTilesCacheUpdate() { m_mustUpdateVisibleTilesCache = true; }

protected:
    void onTileUpdate(const Position& pos);
    void onMapCenterChange(const Position& pos);

    friend class Map;

public:
    
    void lockFirstVisibleFloor(int firstVisibleFloor);
    void unlockFirstVisibleFloor();
    int getLockedFirstVisibleFloor() { return m_lockedFirstVisibleFloor; }

    void setMultifloor(bool enable) { m_multifloor = enable; requestVisibleTilesCacheUpdate(); }
    bool isMultifloor() { return m_multifloor; }

    
    void setVisibleDimension(const Size& visibleDimension);
    Size getVisibleDimension() { return m_visibleDimension; }
    int getTileSize() { return m_tileSize; }
    Point getVisibleCenterOffset() { return m_visibleCenterOffset; }
    int getCachedFirstVisibleFloor() { return m_cachedFirstVisibleFloor; }
    int getCachedLastVisibleFloor() { return m_cachedLastVisibleFloor; }

    
    void setViewMode(ViewMode viewMode);
    ViewMode getViewMode() { return m_viewMode; }
    void optimizeForSize(const Size& visibleSize);

    void setAutoViewMode(bool enable);
    bool isAutoViewModeEnabled() { return m_autoViewMode; }

    
    void followCreature(const CreaturePtr& creature);
    CreaturePtr getFollowingCreature() { return m_followingCreature; }
    bool isFollowingCreature() { return m_followingCreature && m_follow; }

    void setCameraPosition(const Position& pos);
    Position getCameraPosition();

    void setMinimumAmbientLight(float intensity) { m_minimumAmbientLight = intensity; }
    float getMinimumAmbientLight() { return m_minimumAmbientLight; }

    
    void setDrawFlags(Otc::DrawFlags drawFlags) { m_drawFlags = drawFlags; requestVisibleTilesCacheUpdate(); }
    Otc::DrawFlags getDrawFlags() { return m_drawFlags; }

    void setDrawTexts(bool enable) { m_drawTexts = enable; }
    bool isDrawingTexts() { return m_drawTexts; }

    void setDrawNames(bool enable) { m_drawNames = enable; }
    bool isDrawingNames() { return m_drawNames; }

    void setDrawHealthBars(bool enable) { m_drawHealthBars = enable; }
    bool isDrawingHealthBars() { return m_drawHealthBars; }

    void setDrawLights(bool enable);
    bool isDrawingLights() { return m_drawLights; }

    void setDrawManaBar(bool enable) { m_drawManaBar = enable; }
    bool isDrawingManaBar() { return m_drawManaBar; }

    void move(int x, int y);

    void setAnimated(bool animated) { m_animated = animated; requestVisibleTilesCacheUpdate(); }
    bool isAnimating() { return m_animated; }

    void setAddLightMethod(bool add) { m_lightView->setBlendEquation(add ? Painter::BlendEquation_Add : Painter::BlendEquation_Max); }

    void setShader(const PainterShaderProgramPtr& shader, float fadein, float fadeout);
    PainterShaderProgramPtr getShader() { return m_shader; }

    Position getPosition(const Point& point, const Size& mapSize);

    MapViewPtr asMapView() { return static_self_cast<MapView>(); }

private:
    Rect calcFramebufferSource(const Size& destSize);
    int calcFirstVisibleFloor();
    int calcLastVisibleFloor();
    Point transformPositionTo2D(const Position& position, const Position& relativePosition) {
        return Point((m_virtualCenterOffset.x + (position.x - relativePosition.x) - (relativePosition.z - position.z)) * m_tileSize,
                     (m_virtualCenterOffset.y + (position.y - relativePosition.y) - (relativePosition.z - position.z)) * m_tileSize);
    }

    int m_lockedFirstVisibleFloor;
    int m_cachedFirstVisibleFloor;
    int m_cachedLastVisibleFloor;
    int m_tileSize;
    int m_updateTilesPos;
    Size m_drawDimension;
    Size m_visibleDimension;
    Size m_optimizedSize;
    Point m_virtualCenterOffset;
    Point m_visibleCenterOffset;
    Point m_moveOffset;
    Position m_customCameraPosition;
    stdext::boolean<true> m_mustUpdateVisibleTilesCache;
    stdext::boolean<true> m_mustDrawVisibleTilesCache;
    stdext::boolean<true> m_mustCleanFramebuffer;
    stdext::boolean<true> m_multifloor;
    stdext::boolean<true> m_animated;
    stdext::boolean<true> m_autoViewMode;
    stdext::boolean<true> m_drawTexts;
    stdext::boolean<true> m_drawNames;
    stdext::boolean<true> m_drawHealthBars;
    stdext::boolean<false> m_drawLights;
    stdext::boolean<true> m_drawManaBar;
    stdext::boolean<true> m_smooth;

    stdext::boolean<true> m_follow;
    std::vector<TilePtr> m_cachedVisibleTiles;
    std::vector<CreaturePtr> m_cachedFloorVisibleCreatures;
    CreaturePtr m_followingCreature;
    FrameBufferPtr m_framebuffer;
    PainterShaderProgramPtr m_shader;
    ViewMode m_viewMode;
    Otc::DrawFlags m_drawFlags;
    std::vector<Point> m_spiral;
    LightViewPtr m_lightView;
    float m_minimumAmbientLight;
    Timer m_fadeTimer;
    PainterShaderProgramPtr m_nextShader;
    float m_fadeInTime;
    float m_fadeOutTime;
    stdext::boolean<true> m_shaderSwitchDone;

    std::map<uintptr_t, std::vector<LocalEffect>> m_localEffects;
};

#endif
