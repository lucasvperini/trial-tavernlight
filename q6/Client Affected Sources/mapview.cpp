#include "mapview.h"
#include "creature.h"
#include "map.h"
#include "tile.h"
#include "statictext.h"
#include "animatedtext.h"
#include "missile.h"
#include "shadermanager.h"
#include "lightview.h"

#include <framework/graphics/graphics.h>
#include <framework/graphics/image.h>
#include <framework/graphics/framebuffermanager.h>
#include <framework/core/eventdispatcher.h>
#include <framework/core/application.h>
#include <framework/core/resourcemanager.h>

enum {
    NEAR_VIEW_AREA = 1024,
    MID_VIEW_AREA = 4096, 
    FAR_VIEW_AREA = 16384,
    MAX_TILE_DRAWS = NEAR_VIEW_AREA * 7
};

// The MapView class is responsible for rendering a portion of the game map.
// It manages the visible area, drawing of tiles, creatures, effects, and other map elements.
MapView::MapView()
    : m_viewMode(NEAR_VIEW), // Initialize the view mode to NEAR_VIEW, which affects rendering detail.
      m_lockedFirstVisibleFloor(-1), // No floor is locked initially.
      m_cachedFirstVisibleFloor(7), // Default visible floor range.
      m_cachedLastVisibleFloor(7),
      m_updateTilesPos(0), // Position for updating visible tiles.
      m_fadeOutTime(0), // Shader transition fade out time.
      m_fadeInTime(0), // Shader transition fade in time.
      m_minimumAmbientLight(0) // Minimum ambient light level.
{
    // Calculate the optimized size for rendering based on the map's aware range.
    m_optimizedSize = Size(g_map.getAwareRange().horizontal(), g_map.getAwareRange().vertical()) * Otc::TILE_PIXELS;

    // Create a framebuffer for rendering the map view.
    m_framebuffer = g_framebuffers.createFrameBuffer();

    // Set the initial visible dimension of the map view.
    setVisibleDimension(Size(15, 11));

    // Get the default shader for rendering the map.
    m_shader = g_shaders.getDefaultMapShader();
}

MapView::~MapView()
{
#ifndef NDEBUG
    // Ensure the application is not terminated when the MapView is destroyed.
    assert(!g_app.isTerminated());
#endif
}

void MapView::draw(const Rect& rect)
{
    // Update the cache of visible tiles if necessary.
    if (m_mustUpdateVisibleTilesCache || m_updateTilesPos > 0) {
        updateVisibleTilesCache(m_mustUpdateVisibleTilesCache ? 0 : m_updateTilesPos);
    }

    const float scaleFactor = m_tileSize / static_cast<float>(Otc::TILE_PIXELS);
    Position cameraPosition = getCameraPosition();

    int drawFlags = determineDrawFlags();

    if (m_mustDrawVisibleTilesCache || (drawFlags & Otc::DrawAnimations)) {
        drawVisibleTilesToFramebuffer(cameraPosition, scaleFactor, drawFlags);
    }

    handleShaderTransition();

    Rect srcRect = calcFramebufferSource(rect.size());
    Point drawOffset = srcRect.topLeft();

    if (m_shader && g_painter->hasShaders() && g_graphics.shouldUseShaders() && m_viewMode == NEAR_VIEW) {
        applyShader(srcRect, cameraPosition);
    }

    renderFinalFramebuffer(rect, srcRect, drawOffset);

    if (!cameraPosition.isValid()) return;

    applyStretchFactors(rect, srcRect);
    renderCreaturesInformation(rect, cameraPosition, scaleFactor, drawFlags);
    renderLights(rect, srcRect);
    renderStaticAndAnimatedTexts(rect, cameraPosition);
}

void MapView::drawVisibleTilesToFramebuffer(const Position& cameraPosition, float scaleFactor, int drawFlags) {
    m_framebuffer->bind();
    cleanFramebufferIfNeeded();
    drawVisibleTiles(cameraPosition, scaleFactor, drawFlags);
    m_localEffects.clear();
    m_framebuffer->release();
    m_mustDrawVisibleTilesCache = false;
}

int MapView::determineDrawFlags() const
{
    // Initialize drawFlags to zero, which will be used to determine what elements to draw.
    int drawFlags = 0;

    // Check if animations should be forced or shown based on the current view mode.
    if (unlikely(g_map.isForcingAnimations()) || (likely(g_map.isShowingAnimations()) && m_viewMode == NEAR_VIEW)) {
        drawFlags = Otc::DrawAnimations; // Set the flag to draw animations.
    }

    // Determine which elements to draw based on the view mode.
    if (m_viewMode == NEAR_VIEW) {
        // In NEAR_VIEW mode, draw all elements including creatures and effects.
        drawFlags |= Otc::DrawGround | Otc::DrawGroundBorders | Otc::DrawWalls |
                     Otc::DrawItems | Otc::DrawCreatures | Otc::DrawEffects | Otc::DrawMissiles;
    } else {
        // In other view modes, draw only ground, borders, walls, and items.
        drawFlags |= Otc::DrawGround | Otc::DrawGroundBorders | Otc::DrawWalls | Otc::DrawItems;
    }

    return drawFlags; // Return the determined draw flags.
}

void MapView::cleanFramebufferIfNeeded()
{
    // Check if the framebuffer needs to be cleaned.
    if (m_mustCleanFramebuffer) {
        // Define the rectangle to clear, covering the entire draw dimension.
        Rect clearRect = Rect(0, 0, m_drawDimension * m_tileSize);
        g_painter->setColor(Color::black); // Set the color to black for clearing.
        g_painter->drawFilledRect(clearRect); // Draw a filled rectangle to clear the framebuffer.

        // If lights are enabled, reset the lighting.
        if (m_drawLights) {
            resetLighting();
        }
    }
}

void MapView::resetLighting()
{
    // Reset the light view to its initial state.
    m_lightView->reset();
    m_lightView->resize(m_framebuffer->getSize()); // Resize the light view to match the framebuffer size.

    Light ambientLight;
    // Determine the ambient light based on the camera's z position.
    if (getCameraPosition().z <= Otc::SEA_FLOOR) {
        ambientLight = g_map.getLight(); // Get the light from the map if below sea level.
    } else {
        ambientLight.color = 215; // Set a default color for ambient light.
        ambientLight.intensity = 0; // Set intensity to zero above sea level.
    }
    // Ensure the ambient light intensity is at least the minimum ambient light level.
    ambientLight.intensity = std::max<int>(m_minimumAmbientLight * 255, ambientLight.intensity);
    m_lightView->setGlobalLight(ambientLight); // Set the global light in the light view.
}

void MapView::drawVisibleTiles(Position& cameraPosition, float scaleFactor, int drawFlags)
{
    // Iterate over cached visible tiles and draw them.
    auto it = m_cachedVisibleTiles.begin();
    auto end = m_cachedVisibleTiles.end();
    
    // Loop through each visible floor from top to bottom.
    for (int z = m_cachedLastVisibleFloor; z >= m_cachedFirstVisibleFloor; --z) {
        while (it != end) {
            const TilePtr& tile = *it;
            Position tilePos = tile->getPosition();
            if (tilePos.z != z) break; // Stop if the tile is not on the current floor.

            ++it;
            drawTileCreatures(tilePos, scaleFactor, drawFlags); // Draw creatures on the tile.
        }
    }

    drawMissiles(z, scaleFactor, drawFlags); // Draw missiles on the current floor.
}

void MapView::drawTileCreatures(Position& tilePos, float scaleFactor, int drawFlags)
{
    // Get creatures on the specified tile position.
    auto creatures = g_map.getTile(tilePos)->getCreatures();
    for (const CreaturePtr creature : creatures) {
        // Prepare the creature for drawing.
        creature->preDraw(transformPositionTo2D(tilePos, getCameraPosition()), scaleFactor, drawFlags, m_lightView.get());

        // Handle afterimages for the creature.
        for (const auto& afterimage : creature->getAfterimages()) {
            TilePtr afterimageTile = g_map.getTile(afterimage.m_position);
            m_localEffects[(uintptr_t)afterimageTile.get()].push_back(LocalEffect(LocalEffect::LocalEffectType_Afterimage, creature->static_self_cast<Thing>(), afterimage));
        }
    }
}

void MapView::drawMissiles(int z, float scaleFactor, int drawFlags)
{
    // Check if missiles should be drawn.
    if (drawFlags & Otc::DrawMissiles) {
        // Iterate over missiles on the specified floor and draw them.
        for (const MissilePtr& missile : g_map.getFloorMissiles(z)) {
            missile->draw(transformPositionTo2D(missile->getPosition(), getCameraPosition()), scaleFactor, drawFlags & Otc::DrawAnimations, m_lightView.get());
        }
    }
}

void MapView::handleShaderTransition()
{
    float fadeOpacity = 1.0f; // Default opacity is fully opaque.

    // Handle fade out transition if not done.
    if (!m_shaderSwitchDone && m_fadeOutTime > 0) {
        fadeOpacity = 1.0f - (m_fadeTimer.timeElapsed() / m_fadeOutTime);
        if (fadeOpacity < 0.0f) {
            m_shader = m_nextShader; // Switch to the next shader.
            m_nextShader = nullptr;
            m_shaderSwitchDone = true;
            m_fadeTimer.restart(); // Restart the fade timer.
        }
    }

    // Handle fade in transition if shader switch is done.
    if (m_shaderSwitchDone && m_shader && m_fadeInTime > 0) {
        fadeOpacity = std::min<float>(m_fadeTimer.timeElapsed() / m_fadeInTime, 1.0f);
    }

    g_painter->setOpacity(fadeOpacity); // Set the current opacity for rendering.
}

void MapView::applyShader(const Rect& srcRect, const Position& cameraPosition)
{
    // Calculate the framebuffer rectangle and center point.
    Rect framebufferRect = Rect(0, 0, m_drawDimension * m_tileSize);
    Point center = srcRect.center();
    Point globalCoord = Point(cameraPosition.x - m_drawDimension.width() / 2, -(cameraPosition.y - m_drawDimension.height() / 2)) * m_tileSize;
    
    m_shader->bind(); // Bind the shader for use.
    // Set shader outfit values for map center and global coordinates.
    m_shader->setOutfitValue(ShaderManager::MAP_CENTER_COORD, center.x / static_cast<float>(framebufferRect.width()), 1.0f - center.y / static_cast<float>(framebufferRect.height()));
    m_shader->setOutfitValue(ShaderManager::MAP_GLOBAL_COORD, globalCoord.x / static_cast<float>(framebufferRect.height()), globalCoord.y / static_cast<float>(framebufferRect.height()));
    m_shader->setOutfitValue(ShaderManager::MAP_ZOOM, m_tileSize / static_cast<float>(Otc::TILE_PIXELS));
    g_painter->setShaderProgram(m_shader); // Set the shader program for the painter.
}

void MapView::renderFinalFramebuffer(const Rect& rect, const Rect& srcRect, const Point& drawOffset)
{
    glDisable(GL_BLEND); // Disable blending for drawing.
    m_framebuffer->draw(rect, srcRect); // Draw the framebuffer content to the screen.
    g_painter->resetShaderProgram(); // Reset the shader program after drawing.
    g_painter->resetOpacity(); // Reset the opacity to default.
    glEnable(GL_BLEND); // Re-enable blending.
}

void MapView::applyStretchFactors(const Rect& rect, const Rect& srcRect)
{
    // Calculate horizontal and vertical stretch factors based on the destination and source rectangles.
    float horizontalStretchFactor = rect.width() / static_cast<float>(srcRect.width());
    float verticalStretchFactor = rect.height() / static_cast<float>(srcRect.height());
}

void MapView::renderCreaturesInformation(const Rect& rect, const Position& cameraPosition, float scaleFactor, int drawFlags)
{
    // Render creature information if in NEAR_VIEW mode.
    if (m_viewMode == NEAR_VIEW) {
        for (const CreaturePtr& creature : m_cachedFloorVisibleCreatures) {
            if (!creature->canBeSeen()) continue; // Skip creatures that cannot be seen.

            // Calculate offsets and position for drawing creature information.
            PointF jumpOffset = creature->getJumpOffset() * scaleFactor;
            Point creatureOffset = Point(16 - creature->getDisplacementX(), -creature->getDisplacementY() - 2);
            Position pos = creature->getPosition();
            Point p = transformPositionTo2D(pos, cameraPosition) - rect.topLeft();
            p += (creature->getDrawOffset() + creatureOffset) * scaleFactor - Point(stdext::round(jumpOffset.x), stdext::round(jumpOffset.y));
            p.x *= horizontalStretchFactor;
            p.y *= verticalStretchFactor;
            p += rect.topLeft();

            int flags = 0;
            if (m_drawNames) { flags |= Otc::DrawNames; } // Set flag to draw names if enabled.
            if (m_drawHealthBars) { flags |= Otc::DrawBars; } // Set flag to draw health bars if enabled.
            if (m_drawManaBar) { flags |= Otc::DrawManaBar; } // Set flag to draw mana bars if enabled.
            creature->drawInformation(p, g_map.isCovered(pos, m_cachedFirstVisibleFloor), rect, flags); // Draw the creature's information.
        }
    }
}

void MapView::renderLights(const Rect& rect, const Rect& srcRect)
{
    // Render lights if drawing lights is enabled.
    if (m_drawLights) {
        m_lightView->draw(rect, srcRect); // Draw the light view.
    }
}

void MapView::renderStaticAndAnimatedTexts(const Rect& rect, const Position& cameraPosition)
{
    // Render static and animated texts if in NEAR_VIEW mode and drawing texts is enabled.
    if (m_viewMode == NEAR_VIEW && m_drawTexts) {
        for (const StaticTextPtr& staticText : g_map.getStaticTexts()) {
            Position pos = staticText->getPosition();
            // Skip texts that are not on the same z-level as the camera and have no message mode.
            if (pos.z != cameraPosition.z && staticText->getMessageMode() == Otc::MessageNone)
                continue;

            // Calculate position for drawing the static text.
            Point p = transformPositionTo2D(pos, cameraPosition) - rect.topLeft();
            p.x *= horizontalStretchFactor;
            p.y *= verticalStretchFactor;
            p += rect.topLeft();
            staticText->drawText(p, rect); // Draw the static text.
        }

        for (const AnimatedTextPtr& animatedText : g_map.getAnimatedTexts()) {
            Position pos = animatedText->getPosition();
            // Skip animated texts that are not on the same z-level as the camera.
            if (pos.z != cameraPosition.z)
                continue;

            // Calculate position for drawing the animated text.
            Point p = transformPositionTo2D(pos, cameraPosition) - rect.topLeft();
            p.x *= horizontalStretchFactor;
            p.y *= verticalStretchFactor;
            p += rect.topLeft();
            animatedText->drawText(p, rect); // Draw the animated text.
        }
    }
}

void MapView::updateVisibleTilesCache(int start)
{
    // Reset the cache if starting from the beginning.
    if (start == 0) {
        resetCache();
    } else {
        m_mustCleanFramebuffer = false; // No need to clean framebuffer if not starting from zero.
    }

    Position cameraPosition = getCameraPosition();
    if (!cameraPosition.isValid()) return; // Exit if the camera position is invalid.

    bool stop = false;
    m_cachedVisibleTiles.clear(); // Clear the cache of visible tiles.
    m_mustDrawVisibleTilesCache = true; // Indicate that the visible tiles cache must be drawn.
    m_updateTilesPos = 0; // Reset the update position.

    // Process tiles in a spiral pattern from the last visible floor to the first.
    for (int iz = m_cachedLastVisibleFloor; iz >= m_cachedFirstVisibleFloor && !stop; --iz) {
        processTilesInSpiralPattern(start, iz, stop);
    }

    // Clear the spiral if processing is complete.
    if (!stop) {
        m_updateTilesPos = 0;
        m_spiral.clear();
    }

    // Cache visible creatures if starting from zero and in NEAR_VIEW mode.
    if (start == 0 && m_viewMode <= NEAR_VIEW) {
        m_cachedFloorVisibleCreatures = g_map.getSightSpectators(cameraPosition, false);
    }
}

void MapView::resetCache()
{
    // Calculate the first and last visible floors.
    m_cachedFirstVisibleFloor = calcFirstVisibleFloor();
    m_cachedLastVisibleFloor = calcLastVisibleFloor();
    assert(m_cachedFirstVisibleFloor >= 0 && m_cachedLastVisibleFloor >= 0 &&
           m_cachedFirstVisibleFloor <= Otc::MAX_Z && m_cachedLastVisibleFloor <= Otc::MAX_Z);

    // Ensure the last visible floor is not less than the first.
    if (m_cachedLastVisibleFloor < m_cachedFirstVisibleFloor)
        m_cachedLastVisibleFloor = m_cachedFirstVisibleFloor;

    m_cachedFloorVisibleCreatures.clear(); // Clear cached visible creatures.
    m_cachedVisibleTiles.clear(); // Clear cached visible tiles.

    m_mustCleanFramebuffer = true; // Indicate that the framebuffer must be cleaned.
    m_mustDrawVisibleTilesCache = true; // Indicate that the visible tiles cache must be drawn.
    m_mustUpdateVisibleTilesCache = false; // Reset the update cache flag.
    m_updateTilesPos = 0; // Reset the update position.
}

void MapView::processTilesInSpiralPattern(int start, int iz, bool& stop)
{
    // Calculate the number of diagonals to process.
    const int numDiagonals = m_drawDimension.width() + m_drawDimension.height() - 1;
    for (int diagonal = 0; diagonal < numDiagonals && !stop; ++diagonal) {
        int advance = std::max<int>(diagonal - m_drawDimension.height(), 0);
        for (int iy = diagonal - advance, ix = advance; iy >= 0 && ix < m_drawDimension.width() && !stop; --iy, ++ix) {
            if (m_updateTilesPos < start) {
                m_updateTilesPos++;
                continue; // Skip tiles until reaching the start position.
            }

            // Stop if the maximum number of tile draws is exceeded in HUGE_VIEW mode.
            if (static_cast<int>(m_cachedVisibleTiles.size()) > MAX_TILE_DRAWS && m_viewMode >= HUGE_VIEW) {
                stop = true;
                break;
            }

            // Calculate the position of the tile to process.
            Position tilePos = getCameraPosition().translated(ix - m_virtualCenterOffset.x, iy - m_virtualCenterOffset.y);
            tilePos.coveredUp(getCameraPosition().z - iz);
            if (const TilePtr& tile = g_map.getTile(tilePos)) {
                if (!tile->isDrawable()) continue; // Skip non-drawable tiles.

                // Skip tiles that are completely covered.
                if (g_map.isCompletelyCovered(tilePos, m_cachedFirstVisibleFloor)) continue;
                m_cachedVisibleTiles.push_back(tile); // Add the tile to the cache.
            }
            m_updateTilesPos++;
        }
    }
}

void MapView::setVisibleDimension(const Size& visibleDimension) {
    // Check if the visible dimension has changed.
    if (visibleDimension == m_visibleDimension) return;

    // Ensure the visible dimension is odd.
    if (visibleDimension.width() % 2 == 0 || visibleDimension.height() % 2 == 0) {
        g_logger.traceError("Visible dimension must be odd");
        return;
    }

    // Ensure the visible dimension is not too small.
    if (visibleDimension < Size(3, 3)) {
        g_logger.traceError("Reached maximum zoom in");
        return;
    }

    updateGeometry(visibleDimension, m_optimizedSize); // Update the geometry with the new visible dimension.
}

void MapView::setAutoViewMode(bool enable) {
    // Check if the auto view mode has changed.
    if (m_autoViewMode == enable) return;

    m_autoViewMode = enable; // Set the auto view mode.
    if (enable) {
        updateGeometry(m_visibleDimension, m_optimizedSize); // Update the geometry if auto view mode is enabled.
    }
}

void MapView::setViewMode(MapView::ViewMode viewMode) {
    // Check if the view mode has changed.
    if (m_viewMode == viewMode) return;

    m_viewMode = viewMode; // Set the new view mode.
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache.
}

void MapView::lockFirstVisibleFloor(int firstVisibleFloor) {
    // Check if the first visible floor lock has changed.
    if (m_lockedFirstVisibleFloor == firstVisibleFloor) return;

    m_lockedFirstVisibleFloor = firstVisibleFloor; // Lock the first visible floor.
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache.
}

void MapView::unlockFirstVisibleFloor() {
    // Check if the first visible floor is already unlocked.
    if (m_lockedFirstVisibleFloor == -1) return;

    m_lockedFirstVisibleFloor = -1; // Unlock the first visible floor.
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache.
}

void MapView::followCreature(const CreaturePtr& creature) {
    // Check if the creature to follow has changed.
    if (m_followingCreature == creature) return;

    m_follow = true; // Enable following mode.
    m_followingCreature = creature; // Set the creature to follow.
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache.
}

void MapView::setCameraPosition(const Position& pos) {
    // Check if the camera position has changed.
    if (m_customCameraPosition == pos) return;

    m_follow = false; // Disable following mode.
    m_customCameraPosition = pos; // Set the new camera position.
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache.
}

void MapView::optimizeForSize(const Size& visibleSize) {
    updateGeometry(m_visibleDimension, visibleSize); // Update the geometry for the specified size.
}

void MapView::onTileUpdate(const Position&) {
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache when a tile is updated.
}

void MapView::onMapCenterChange(const Position&) {
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache when the map center changes.
}

void MapView::updateGeometry(const Size& visibleDimension, const Size& optimizedSize) {
    // Define possible tile sizes for rendering.
    static const int tileSizes[] = {1, 2, 4, 8, 16, 32};
    Size bufferSize;
    int tileSize = 0;

    // Determine the appropriate tile size based on the visible and optimized sizes.
    for (int candidateTileSize : tileSizes) {
        bufferSize = (visibleDimension + Size(3, 3)) * candidateTileSize;
        if (bufferSize.width() > g_graphics.getMaxTextureSize() ||
            bufferSize.height() > g_graphics.getMaxTextureSize()) break;

        tileSize = candidateTileSize;
        if (optimizedSize.width() < bufferSize.width() - 3 * candidateTileSize &&
            optimizedSize.height() < bufferSize.height() - 3 * candidateTileSize) break;
    }

    // Check if a valid tile size was found.
    if (tileSize == 0) {
        g_logger.traceError("Reached maximum zoom out");
        return;
    }

    // Calculate the draw dimension and center offsets.
    Size drawDimension = visibleDimension + Size(3, 3);
    Point virtualCenterOffset = (drawDimension / 2 - Size(1, 1)).toPoint();
    Point visibleCenterOffset = virtualCenterOffset;

    // Determine the view mode based on the tile size and area.
    ViewMode viewMode = m_autoViewMode ? determineViewMode(tileSize, visibleDimension.area()) : m_viewMode;
    m_multifloor = viewMode < FAR_VIEW; // Set multifloor mode based on the view mode.

    // Update the view mode, dimensions, and framebuffer size.
    m_viewMode = viewMode;
    m_visibleDimension = visibleDimension;
    m_drawDimension = drawDimension;
    m_tileSize = tileSize;
    m_virtualCenterOffset = virtualCenterOffset;
    m_visibleCenterOffset = visibleCenterOffset;
    m_optimizedSize = optimizedSize;
    m_framebuffer->resize(bufferSize);
    requestVisibleTilesCacheUpdate(); // Request an update to the visible tiles cache.
}

MapView::ViewMode MapView::determineViewMode(int tileSize, int area) {
    // Determine the view mode based on the tile size and area.
    if (tileSize >= 32 && area <= NEAR_VIEW_AREA) return NEAR_VIEW;
    if (tileSize >= 16 && area <= MID_VIEW_AREA) return MID_VIEW;
    if (tileSize >= 8 && area <= FAR_VIEW_AREA) return FAR_VIEW;
    return HUGE_VIEW; // Default to HUGE_VIEW if no other conditions are met.
}

Position MapView::getPosition(const Point& point, const Size& mapSize) {
    Position camPos = getCameraPosition();
    if (!camPos.isValid()) return Position(); // Return an invalid position if the camera position is invalid.

    // Calculate the source rectangle and scale factors.
    Rect srcRect = calcFramebufferSource(mapSize);
    float scaleX = static_cast<float>(srcRect.width()) / mapSize.width();
    float scaleY = static_cast<float>(srcRect.height()) / mapSize.height();

    // Calculate the framebuffer position and offset.
    Point fbPos = Point(point.x * scaleX, point.y * scaleY);
    Point offset = (fbPos + srcRect.topLeft()) / m_tileSize;
    Point tileCoord = getVisibleCenterOffset() - m_drawDimension.toPoint() + offset + Point(2,2);

    // Return an invalid position if the calculated coordinates are out of bounds.
    if (tileCoord.x + camPos.x < 0 && tileCoord.y + camPos.y < 0) return Position();

    // Calculate the final position and return it if valid.
    Position finalPos = Position(tileCoord.x, tileCoord.y, 0) + camPos;
    return finalPos.isValid() ? finalPos : Position();
}

void MapView::move(int x, int y) {
    m_moveOffset += Point(x, y); // Update the move offset with the specified values.

    int dx = m_moveOffset.x / 32;
    int dy = m_moveOffset.y / 32;
    bool updateRequired = false;

    // Update the camera position based on the move offset.
    if (dx != 0) {
        m_customCameraPosition.x += dx;
        m_moveOffset.x %= 32;
        updateRequired = true;
    }
    if (dy != 0) {
        m_customCameraPosition.y += dy;
        m_moveOffset.y %= 32;
        updateRequired = true;
    }

    // Request an update to the visible tiles cache if the camera position was updated.
    if (updateRequired) requestVisibleTilesCacheUpdate();
}

Rect MapView::calcFramebufferSource(const Size& destSize) {
    // Calculate the scale factor and draw offset.
    float scaleFactor = m_tileSize / static_cast<float>(Otc::TILE_PIXELS);
    Point drawOffset = ((m_drawDimension - m_visibleDimension - Size(1,1)).toPoint() / 2) * m_tileSize;

    // Adjust the draw offset if following a creature or if there is a move offset.
    if (isFollowingCreature()) {
        drawOffset += m_followingCreature->getWalkOffset() * scaleFactor;
    } else if (!m_moveOffset.isNull()) {
        drawOffset += m_moveOffset * scaleFactor;
    }

    // Calculate the source size and adjust the draw offset.
    Size srcSize = destSize;
    Size srcVisible = m_visibleDimension * m_tileSize;
    srcSize.scale(srcVisible, Fw::KeepAspectRatio);

    drawOffset += Point((srcVisible.width() - srcSize.width()) / 2, (srcVisible.height() - srcSize.height()) / 2);

    return Rect(drawOffset, srcSize); // Return the calculated source rectangle.
}

int MapView::calcFirstVisibleFloor() {
    // Return the locked first visible floor if it is set.
    if (m_lockedFirstVisibleFloor != -1) return m_lockedFirstVisibleFloor;

    Position camPos = getCameraPosition();
    if (!camPos.isValid()) return 7; // Return a default floor if the camera position is invalid.

    // Return the camera's z position if multifloor mode is disabled.
    if (!m_multifloor) return camPos.z;

    // Calculate the first visible floor based on the camera's z position.
    int firstFloor = camPos.z > Otc::SEA_FLOOR ? std::max<int>(camPos.z - Otc::AWARE_UNDEGROUND_FLOOR_RANGE, Otc::UNDERGROUND_FLOOR) : 0;

    // Check surrounding positions to determine the first visible floor.
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (firstFloor >= camPos.z) break;
            Position pos = camPos.translated(dx, dy);
            if ((dx == 0 && dy == 0) || (std::abs(dx) != std::abs(dy) && g_map.isLookPossible(pos))) {
                Position upper = pos, covered = pos;
                while (covered.coveredUp() && upper.up() && upper.z >= firstFloor) {
                    if (TilePtr tile = g_map.getTile(upper); tile && tile->limitsFloorsView(!g_map.isLookPossible(pos))) {
                        firstFloor = upper.z + 1;
                        break;
                    }
                    if (TilePtr tile = g_map.getTile(covered); tile && tile->limitsFloorsView(g_map.isLookPossible(pos))) {
                        firstFloor = covered.z + 1;
                        break;
                    }
                }
            }
        }
    }
    return stdext::clamp<int>(firstFloor, 0, Otc::MAX_Z); // Clamp the first floor to valid bounds.
}

int MapView::calcLastVisibleFloor() {
    // Return the first visible floor if multifloor mode is disabled.
    if (!m_multifloor) return calcFirstVisibleFloor();

    Position camPos = getCameraPosition();
    if (!camPos.isValid()) return 7; // Return a default floor if the camera position is invalid.

    // Calculate the last visible floor based on the camera's z position.
    int lastFloor = camPos.z > Otc::SEA_FLOOR ? camPos.z + Otc::AWARE_UNDEGROUND_FLOOR_RANGE : Otc::SEA_FLOOR;
    if (m_lockedFirstVisibleFloor != -1) lastFloor = std::max<int>(m_lockedFirstVisibleFloor, lastFloor);
    
    return stdext::clamp<int>(lastFloor, 0, Otc::MAX_Z); // Clamp the last floor to valid bounds.
}

Position MapView::getCameraPosition() {
    // Return the position of the following creature if following, otherwise return the custom camera position.
    return isFollowingCreature() ? m_followingCreature->getPosition() : m_customCameraPosition;
}

void MapView::setShader(const PainterShaderProgramPtr& shader, float fadein, float fadeout) {
    // Check if the shader is already set or if a shader switch is pending.
    if ((m_shader == shader && m_shaderSwitchDone) || (m_nextShader == shader && !m_shaderSwitchDone)) return;

    // Set the next shader and initiate a fade out transition if necessary.
    if (fadeout > 0.0f && m_shader) {
        m_nextShader = shader;
        m_shaderSwitchDone = false;
    } else {
        m_shader = shader; // Set the shader directly if no fade out is needed.
        m_nextShader = nullptr;
        m_shaderSwitchDone = true;
    }
    m_fadeTimer.restart(); // Restart the fade timer.
    m_fadeInTime = fadein; // Set the fade in time.
    m_fadeOutTime = fadeout; // Set the fade out time.
}

void MapView::setDrawLights(bool enable) {
    // Check if the draw lights setting has changed.
    if (m_drawLights == enable) return;
    m_drawLights = enable; // Set the draw lights flag.
    m_lightView = enable ? LightViewPtr(new LightView) : nullptr; // Create or destroy the light view based on the setting.
}