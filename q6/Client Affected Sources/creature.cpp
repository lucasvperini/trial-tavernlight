#include "creature.h"
#include "thingtypemanager.h"
#include "localplayer.h"
#include "map.h"
#include "tile.h"
#include "item.h"
#include "game.h"
#include "effect.h"
#include "luavaluecasts.h"
#include "lightview.h"
#include "uimap.h"

#include <framework/graphics/graphics.h>
#include <framework/core/eventdispatcher.h>
#include <framework/core/clock.h>

#include <framework/graphics/paintershaderprogram.h>
#include <framework/graphics/ogl/painterogl2_shadersources.h>
#include <framework/graphics/texturemanager.h>
#include <framework/graphics/framebuffermanager.h>
#include <framework/ui/uimanager.h>
#include "spritemanager.h"

Creature::Creature() : Thing()
{
    // Initialize creature attributes with default values
    initializeAttributes();
}

void Creature::initializeAttributes()
{
    // Set default values for creature attributes
    m_id = 0;
    m_healthPercent = 100;
    m_speed = 200;
    m_direction = Otc::South;
    m_walkAnimationPhase = 0;
    m_walkedPixels = 0;
    m_walkTurnDirection = Otc::InvalidDirection;
    m_skull = Otc::SkullNone;
    m_shield = Otc::ShieldNone;
    m_emblem = Otc::EmblemNone;
    m_type = Proto::CreatureTypeUnknown;
    m_icon = Otc::NpcIconNone;
    m_lastStepDirection = Otc::InvalidDirection;
    m_nameCache.setFont(g_fonts.getFont("verdana-11px-rounded"));
    m_nameCache.setAlign(Fw::AlignTopCenter);
    m_footStep = 0;
    m_speedFormula.fill(-1);
    m_outfitColor = Color::white;
    m_isDashing = false;
    m_afterimagesClearTimer = Timer();
}

void Creature::preDraw(const Point& dest, float scaleFactor, bool animate, LightView* lightView)
{
    // Check if the outfit category is a creature before drawing
    if (m_outfit.getCategory() != ThingCategoryCreature) {
        return;
    }

    // Calculate patterns for drawing based on direction and mount status
    int zPattern = calculateZPattern();
    int xPattern = calculateXPattern();

    // Update afterimages if the position has changed
    updateAfterimages(zPattern, xPattern);
}

int Creature::calculateZPattern() const
{
    // Determine Z pattern based on whether the creature is mounted
    return m_outfit.getMount() != 0 ? std::min<int>(1, getNumPatternZ() - 1) : 0;
}

int Creature::calculateXPattern() const
{
    // Determine X pattern based on the creature's direction
    switch (m_direction) {
        case Otc::NorthEast:
        case Otc::SouthEast:
            return Otc::East;
        case Otc::NorthWest:
        case Otc::SouthWest:
            return Otc::West;
        default:
            return m_direction;
    }
}

void Creature::updateAfterimages(int zPattern, int xPattern)
{
    // If the creature has moved, update the afterimages for dashing effect
    if (m_position != m_lastPosition) {
        if (m_isDashing) {
            // Calculate offset for afterimages based on movement direction
            Point direction = m_lastPosition - m_position;
            Point offset = direction * (Otc::TILE_PIXELS / 2);
            // Create afterimages at current and last positions
            m_afterimages.emplace_back(LocalEffect::Afterimage(m_position, offset, xPattern, zPattern, 0, 400.0f));
            m_afterimages.emplace_back(LocalEffect::Afterimage(m_lastPosition, Point(0, 0), xPattern, zPattern, 0, 350.0f));
        }
        m_lastPosition = m_position; // Update last position
    }
}

void Creature::draw(const Point& dest, float scaleFactor, bool animate, LightView* lightView)
{
    // Check if the creature can be seen before drawing
    if (!canBeSeen()) {
        return;
    }

    // Calculate animation offset based on whether the creature is walking
    Point animationOffset = calculateAnimationOffset(animate);

    // Draw bounding squares around the creature if necessary
    drawBoundingSquares(dest, scaleFactor, animate, animationOffset);

    // Draw the creature's outfit
    internalDrawOutfit(dest + animationOffset * scaleFactor, scaleFactor, animate, animate, m_direction);
    m_footStepDrawn = true; // Mark footstep as drawn

    // Add light source if applicable
    addLightSource(dest, scaleFactor, animationOffset, lightView);
}

Point Creature::calculateAnimationOffset(bool animate) const
{
    // Return walk offset if animating, otherwise no offset
    return animate ? m_walkOffset : Point(0, 0);
}

void Creature::drawBoundingSquares(const Point& dest, float scaleFactor, bool animate, const Point& animationOffset)
{
    // Draw timed and static squares around the creature if enabled
    if (m_showTimedSquare && animate) {
        g_painter->setColor(m_timedSquareColor);
        g_painter->drawBoundingRect(Rect(dest + (animationOffset - getDisplacement() + 2) * scaleFactor, Size(28, 28) * scaleFactor), std::max<int>((int)(2 * scaleFactor), 1));
        g_painter->setColor(Color::white);
    }

    if (m_showStaticSquare && animate) {
        g_painter->setColor(m_staticSquareColor);
        g_painter->drawBoundingRect(Rect(dest + (animationOffset - getDisplacement()) * scaleFactor, Size(Otc::TILE_PIXELS, Otc::TILE_PIXELS) * scaleFactor), std::max<int>((int)(2 * scaleFactor), 1));
        g_painter->setColor(Color::white);
    }
}

void Creature::addLightSource(const Point& dest, float scaleFactor, const Point& animationOffset, LightView* lightView)
{
    // Add a light source to the light view if the creature emits light
    if (!lightView) {
        return;
    }

    Light light = calculateLight();

    if (light.intensity > 0) {
        lightView->addLightSource(dest + (animationOffset + Point(16, 16)) * scaleFactor, scaleFactor, light);
    }
}

Light Creature::calculateLight() const
{
    // Calculate the light emitted by the creature
    Light light = rawGetThingType()->getLight();
    if (m_light.intensity != light.intensity || m_light.color != light.color) {
        light = m_light;
    }

    // Adjust light intensity for local player in dark areas
    if (isLocalPlayer() && (g_map.getLight().intensity < 64 || m_position.z > Otc::SEA_FLOOR)) {
        light.intensity = std::max<uint8>(light.intensity, 3);
        if (light.color == 0 || light.color > 215) {
            light.color = 215;
        }
    }

    return light;
}

void Creature::internalDrawOutfit(Point dest, float scaleFactor, bool animateWalk, bool animateIdle, Otc::Direction direction, LightView* lightView)
{
    // Save the current painter state and set color for drawing the outfit
    g_painter->saveState();
    g_painter->setColor(m_outfitColor);
    g_painter->applyPaintType(Painter::PaintType_Creature);

    // Draw the outfit based on its category
    if (m_outfit.getCategory() == ThingCategoryCreature) {
        drawCreatureOutfit(dest, scaleFactor, animateWalk, animateIdle, direction, lightView);
    } else {
        drawNonCreatureOutfit(dest, scaleFactor, animateIdle, lightView);
    }

    // Reset painter color and restore state
    g_painter->resetColor();
    g_painter->restoreSavedState();
}

void Creature::drawCreatureOutfit(Point dest, float scaleFactor, bool animateWalk, bool animateIdle, Otc::Direction direction, LightView* lightView)
{
    // Determine the animation phase for the outfit
    int animationPhase = determineAnimationPhase(animateWalk, animateIdle);
    int xPattern = calculateXPattern();
    int zPattern = calculateZPatternForMount(dest, scaleFactor);

    // Adjust destination for jump offset
    PointF jumpOffset = m_jumpOffset * scaleFactor;
    dest -= Point(stdext::round(jumpOffset.x), stdext::round(jumpOffset.y));

    // Draw each layer of the outfit
    for (int yPattern = 0; yPattern < getNumPatternY(); yPattern++) {
        if (yPattern > 0 && !(m_outfit.getAddons() & (1 << (yPattern - 1)))) {
            continue;
        }

        auto datType = rawGetThingType();

        // Apply dash effect if dashing
        applyDashEffect(datType, dest, scaleFactor, xPattern, yPattern, zPattern, animationPhase, lightView);
        // Draw the outfit layers
        drawOutfitLayers(datType, dest, scaleFactor, xPattern, yPattern, zPattern, animationPhase);
    }
}

int Creature::determineAnimationPhase(bool animateWalk, bool animateIdle) const
{
    // Determine the animation phase based on walking and idle states
    if (animateWalk) {
        return m_walkAnimationPhase;
    }

    if (isAnimateAlways() && animateIdle) {
        int ticksPerFrame = 1000 / getAnimationPhases();
        return (g_clock.millis() % (ticksPerFrame * getAnimationPhases())) / ticksPerFrame;
    }

    return 0;
}

int Creature::calculateZPatternForMount(Point& dest, float scaleFactor) const
{
    // Adjust destination and calculate Z pattern if the creature is mounted
    if (m_outfit.getMount() != 0) {
        auto datType = g_things.rawGetThingType(m_outfit.getMount(), ThingCategoryCreature);
        dest -= datType->getDisplacement() * scaleFactor;
        datType->draw(dest, scaleFactor, 0, calculateXPattern(), 0, 0, determineAnimationPhase(false, false), nullptr);
        dest += getDisplacement() * scaleFactor;
        return std::min<int>(1, getNumPatternZ() - 1);
    }

    return 0;
}

void Creature::applyDashEffect(const std::shared_ptr<ThingType>& datType, Point& dest, float scaleFactor, int xPattern, int yPattern, int zPattern, int animationPhase, LightView* lightView)
{
    // Apply a visual effect for dashing creatures
    if (isDashing()) {
        g_painter->setBrushConfiguration(BrushConfiguration("u_IsDashing", 1));
        g_painter->flushBrushConfigurations(Painter::PaintType_Creature);

        datType->draw(dest, scaleFactor, 0, xPattern, yPattern, zPattern, animationPhase, yPattern == 0 ? lightView : nullptr);

        g_painter->setBrushConfiguration(BrushConfiguration("u_IsDashing", 0));
        g_painter->flushBrushConfigurations(Painter::PaintType_Creature);
    }
}

void Creature::drawOutfitLayers(const std::shared_ptr<ThingType>& datType, Point& dest, float scaleFactor, int xPattern, int yPattern, int zPattern, int animationPhase)
{
    // Draw the base layer of the outfit
    datType->draw(dest, scaleFactor, 0, xPattern, yPattern, zPattern, animationPhase, nullptr);

    // Draw additional layers with color if the outfit has multiple layers
    if (getLayers() > 1) {
        Color oldColor = g_painter->getColor();
        Painter::CompositionMode oldComposition = g_painter->getCompositionMode();
        g_painter->setCompositionMode(Painter::CompositionMode_Multiply);

        // Draw each layer with its respective color
        g_painter->setColor(m_outfit.getHeadColor());
        datType->draw(dest, scaleFactor, SpriteMaskYellow, xPattern, yPattern, zPattern, animationPhase);
        g_painter->setColor(m_outfit.getBodyColor());
        datType->draw(dest, scaleFactor, SpriteMaskRed, xPattern, yPattern, zPattern, animationPhase);
        g_painter->setColor(m_outfit.getLegsColor());
        datType->draw(dest, scaleFactor, SpriteMaskGreen, xPattern, yPattern, zPattern, animationPhase);
        g_painter->setColor(m_outfit.getFeetColor());
        datType->draw(dest, scaleFactor, SpriteMaskBlue, xPattern, yPattern, zPattern, animationPhase);

        // Restore previous color and composition mode
        g_painter->setColor(oldColor);
        g_painter->setCompositionMode(oldComposition);
    }
}

void Creature::drawNonCreatureOutfit(Point dest, float scaleFactor, bool animateIdle, LightView* lightView)
{
    // Draw non-creature outfits such as items or effects
    auto* type = g_things.rawGetThingType(m_outfit.getAuxId(), m_outfit.getCategory());
    int animationPhase = determineNonCreatureAnimationPhase(animateIdle, type);
    auto offset = dest - (getDisplacement() * scaleFactor);
    type->draw(offset, scaleFactor, 0, 0, 0, 0, animationPhase, lightView);
}

int Creature::determineNonCreatureAnimationPhase(bool animateIdle, ThingType* type) const
{
    // Determine the animation phase for non-creature outfits
    int animationPhase = 0;
    int animationPhases = type->getAnimationPhases();
    int ticksPerFrame = (m_outfit.getCategory() == ThingCategoryEffect) ? Otc::INVISIBLE_TICKS_PER_FRAME : Otc::ITEM_TICKS_PER_FRAME;

    if (m_outfit.getCategory() == ThingCategoryEffect)
        animationPhases = std::max<int>(1, animationPhases - 2);

    if (animationPhases > 1)
        animationPhase = animateIdle ? (g_clock.millis() % (ticksPerFrame * animationPhases)) / ticksPerFrame : animationPhases - 1;

    if (m_outfit.getCategory() == ThingCategoryEffect)
        animationPhase = std::min<int>(animationPhase + 1, animationPhases);

    return animationPhase;
}

void Creature::postDraw()
{
    // Clear expired afterimages periodically
    if (m_afterimagesClearTimer.ticksElapsed() >= 33.33f) {
        m_afterimages.erase(std::remove_if(m_afterimages.begin(), m_afterimages.end(), [](const LocalEffect::Afterimage& ai) {
            return ai.m_timer.ticksElapsed() >= ai.m_duration;
        }), m_afterimages.end());

        m_afterimagesClearTimer.restart();
    }
}

void Creature::drawOutfit(const Rect& destRect, bool resize)
{
    // Determine the exact size of the outfit based on its category
    int exactSize = (m_outfit.getCategory() == ThingCategoryCreature) ? getExactSize() :
                    g_things.rawGetThingType(m_outfit.getAuxId(), m_outfit.getCategory())->getExactSize();

    // Calculate the frame size, adjusting if resizing is not required
    int frameSize = (!resize) ? std::max<int>(exactSize * 0.75f, 1.5f * Otc::TILE_PIXELS) : exactSize;
    if (!frameSize) return; // Exit if frame size is zero

    // Check if Frame Buffer Object (FBO) can be used for rendering
    if (g_graphics.canUseFBO()) {
        auto outfitBuffer = g_framebuffers.getTemporaryFrameBuffer();
        outfitBuffer->resize(Size(frameSize, frameSize));
        outfitBuffer->bind();

        // Enable alpha writing and clear the buffer
        g_painter->setAlphaWriting(true);
        g_painter->clear(Color::alpha);

        // Draw the outfit internally with the specified parameters
        internalDrawOutfit(Point(frameSize - Otc::TILE_PIXELS, frameSize - Otc::TILE_PIXELS) + getDisplacement(), 1, false, true, Otc::South);
        outfitBuffer->release();
        outfitBuffer->draw(destRect, Rect(0, 0, frameSize, frameSize));
    } else {
        // Calculate the scale factor for drawing without FBO
        float scaleFactor = destRect.width() / static_cast<float>(frameSize);
        Point dest = destRect.bottomRight() - (Point(Otc::TILE_PIXELS, Otc::TILE_PIXELS) - getDisplacement()) * scaleFactor;
        internalDrawOutfit(dest, scaleFactor, false, true, Otc::South);
    }
}

void Creature::drawAfterimage(Point& dest, float scaleFactor, const LocalEffect::Afterimage& afterimage)
{
    // Set the color for the outfit
    g_painter->setColor(m_outfitColor);

    // Check if the outfit category is a creature
    if (m_outfit.getCategory() == ThingCategoryCreature) {
        // Adjust destination for jump offset
        dest -= Point(stdext::round(m_jumpOffset.x * scaleFactor), stdext::round(m_jumpOffset.y * scaleFactor));

        // Iterate over each pattern layer
        for (int yPattern = 0; yPattern < getNumPatternY(); ++yPattern) {
            // Skip layers not included in the outfit's addons
            if (yPattern > 0 && !(m_outfit.getAddons() & (1 << (yPattern - 1))))
                continue;

            // Set opacity based on the afterimage's timer
            float oldOpacity = g_painter->getOpacity();
            g_painter->setOpacity(1 - std::max(afterimage.m_timer.ticksElapsed() / afterimage.m_duration, 0.0f));
            auto* datType = rawGetThingType();

            // Draw the current layer of the outfit
            datType->draw(dest, scaleFactor, 0, afterimage.m_xPattern, yPattern, afterimage.m_zPattern, afterimage.m_animationPhase, nullptr);
            g_painter->setOpacity(oldOpacity);

            // Draw additional layers with color if the outfit has multiple layers
            if (getLayers() > 1) {
                auto oldColor = g_painter->getColor();
                auto oldComposition = g_painter->getCompositionMode();

                g_painter->setCompositionMode(Painter::CompositionMode_Multiply);

                // Helper function to draw each layer with its respective color
                auto drawLayerColor = [&](Color color, SpriteMask mask) {
                    g_painter->setColor(color);
                    datType->draw(dest, scaleFactor, mask, afterimage.m_xPattern, yPattern, afterimage.m_zPattern, afterimage.m_animationPhase);
                };

                // Draw each layer with its respective color
                drawLayerColor(m_outfit.getHeadColor(), SpriteMaskYellow);
                drawLayerColor(m_outfit.getBodyColor(), SpriteMaskRed);
                drawLayerColor(m_outfit.getLegsColor(), SpriteMaskGreen);
                drawLayerColor(m_outfit.getFeetColor(), SpriteMaskBlue);

                // Restore previous color and composition mode
                g_painter->setColor(oldColor);
                g_painter->setCompositionMode(oldComposition);
            }
        }
    }
    // Reset the painter color to default
    g_painter->resetColor();
}

void Creature::drawInformation(const Point& point, bool useGray, const Rect& parentRect, int drawFlags)
{
    // Exit if the creature's health is below 1
    if (m_healthPercent < 1) return;

    // Determine the fill color based on whether gray is used
    auto fillColor = (useGray) ? Color(96, 96, 96) : m_informationColor;

    // Define the background rectangle for health and mana bars
    Rect backgroundRect(point.x - 13.5f, point.y, 27, 4);
    backgroundRect.bind(parentRect);

    // Calculate the text rectangle for the creature's name
    auto nameSize = m_nameCache.getTextSize();
    Rect textRect(point.x - nameSize.width() / 2.0f, point.y - 12, nameSize);
    textRect.bind(parentRect);

    // Determine the offset for local players
    int offset = (isLocalPlayer()) ? 24 : 12;

    // Adjust the position of the background and text rectangles
    if (textRect.top() == parentRect.top())
        backgroundRect.moveTop(textRect.top() + offset);
    if (backgroundRect.bottom() == parentRect.bottom())
        textRect.moveTop(backgroundRect.top() - offset);

    // Define the health rectangle and set its width based on health percentage
    Rect healthRect = backgroundRect.expanded(-1);
    healthRect.setWidth((m_healthPercent / 100.0f) * 25);

    // Draw health and mana bars if the flags are set
    if (drawFlags & Otc::DrawBars && (!isNpc() || !g_game.getFeature(Otc::GameHideNpcNames))) {
        g_painter->setColor(Color::black);
        g_painter->drawFilledRect(backgroundRect);
        g_painter->setColor(fillColor);
        g_painter->drawFilledRect(healthRect);

        // Draw mana bar for local players
        if ((drawFlags & Otc::DrawManaBar) && isLocalPlayer()) {
            LocalPlayerPtr player = g_game.getLocalPlayer();
            if (player) {
                backgroundRect.moveTop(backgroundRect.bottom());

                g_painter->setColor(Color::black);
                g_painter->drawFilledRect(backgroundRect);

                Rect manaRect = backgroundRect.expanded(-1);
                double maxMana = player->getMaxMana();
                manaRect.setWidth((maxMana > 0) ? player->getMana() / maxMana * 25 : 25);

                g_painter->setColor(Color::blue);
                g_painter->drawFilledRect(manaRect);
            }
        }
    }

    // Draw the creature's name if the flag is set
    if (drawFlags & Otc::DrawNames) {
        g_painter->setColor(fillColor);
        m_nameCache.draw(textRect);
    }

    // Helper function to draw icons (skull, shield, emblem, etc.)
    auto drawIcon = [&](Otc::IconType type, const TexturePtr& texture, int xOffset, int yOffset) {
        if (type != Otc::None && texture) {
            g_painter->setColor(Color::white);
            Rect iconRect(backgroundRect.x() + xOffset, backgroundRect.y() + yOffset, texture->getSize());
            g_painter->drawTexturedRect(iconRect, texture);
        }
    };

    // Draw various icons associated with the creature
    drawIcon(m_skull, m_skullTexture, 25.5, 5);
    drawIcon(m_shield, m_shieldTexture, 13.5, 5);
    drawIcon(m_emblem, m_emblemTexture, 25.5, 16);
    drawIcon(m_type, m_typeTexture, 37.5, 16);
    drawIcon(m_icon, m_iconTexture, 25.5, 5);
}

void Creature::turn(Otc::Direction direction)
{
    // Set the creature's direction if not walking, otherwise set the turn direction
    if (!m_walking)
        setDirection(direction);
    else
        m_walkTurnDirection = direction;
}

void Creature::walk(const Position& previousPosition, const Position& currentPosition)
{
    // Exit if the creature hasn't moved
    if (previousPosition == currentPosition) return;

    // Determine the direction of the last step
    m_lastStepDirection = previousPosition.getDirectionFromPosition(currentPosition);
    m_lastStepFromPosition = previousPosition;
    m_lastStepToPosition = currentPosition;
    setDirection(m_lastStepDirection);

    // Start walking and reset relevant timers and variables
    m_walking = true;
    m_walkTimer.restart();
    m_walkedPixels = 0;

    // Cancel any existing walk finish animation event
    if (m_walkFinishAnimEvent) {
        m_walkFinishAnimEvent->cancel();
        m_walkFinishAnimEvent = nullptr;
    }

    // Reset the walk turn direction and schedule the next walk update
    m_walkTurnDirection = Otc::InvalidDirection;
    nextWalkUpdate();
}

void Creature::stopWalk()
{
    // Terminate walking if the creature is currently walking
    if (!m_walking) return;
    terminateWalk();
}

void Creature::jump(int height, int duration)
{
    // Start a jump if the creature is not already jumping
    if (!m_jumpOffset.isNull()) return;

    m_jumpTimer.restart();
    m_jumpHeight = height;
    m_jumpDuration = duration;
    updateJump();
}

void Creature::updateJump()
{
    // Calculate the elapsed time and physics for the jump
    int elapsedTime = m_jumpTimer.ticksElapsed();
    double acceleration = -4.0 * m_jumpHeight / (m_jumpDuration * m_jumpDuration);
    double initialVelocity = 4.0 * m_jumpHeight / m_jumpDuration;
    
    double height = acceleration * elapsedTime * elapsedTime + initialVelocity * elapsedTime;
    int roundedHeight = stdext::round(height);
    int midJumpDuration = m_jumpDuration / 2;

    // Update the jump offset and schedule the next update if the jump is ongoing
    if (elapsedTime < m_jumpDuration) {
        m_jumpOffset = PointF(height, height);
        int adjustment = elapsedTime < midJumpDuration ? 1 : -1;

        int nextTime, i = 1;
        do {
            nextTime = stdext::round((-initialVelocity + std::sqrt(std::max<double>(initialVelocity * initialVelocity + 4 * acceleration * (roundedHeight + adjustment * i), 0.0)) * adjustment) / (2 * acceleration));
            ++i;
        } while (nextTime - elapsedTime == 0 && i < 3);

        auto self = static_self_cast<Creature>();
        g_dispatcher.scheduleEvent([self] { self->updateJump(); }, nextTime - elapsedTime);
    } else {
        // Reset the jump offset when the jump is complete
        m_jumpOffset = PointF(0, 0);
    }
}

void Creature::onPositionChange(const Position& newPosition, const Position& oldPosition)
{
    // Notify Lua scripts of the position change
    callLuaField("onPositionChange", newPosition, oldPosition);
}

void Creature::onAppear()
{
    // Handle the creature's appearance, canceling any disappear event
    if (m_disappearEvent) {
        m_disappearEvent->cancel();
        m_disappearEvent = nullptr;
    }

    // Handle reappearance logic based on previous state and position
    if (m_removed) {
        stopWalk();
        m_removed = false;
        callLuaField("onAppear");
    } else if (m_oldPosition != m_position && m_oldPosition.isInRange(m_position, 1, 1) && m_allowAppearWalk) {
        m_allowAppearWalk = false;
        walk(m_oldPosition, m_position);
        callLuaField("onWalk", m_oldPosition, m_position);
    } else if (m_oldPosition != m_position) {
        stopWalk();
        callLuaField("onDisappear");
        callLuaField("onAppear");
    }
}

void Creature::onDisappear()
{
    // Schedule a disappear event to handle the creature's removal
    if (m_disappearEvent) {
        m_disappearEvent->cancel();
    }

    m_oldPosition = m_position;
    auto self = static_self_cast<Creature>();
    m_disappearEvent = g_dispatcher.addEvent([self] {
        self->m_removed = true;
        self->stopWalk();
        self->callLuaField("onDisappear");

        if (!self->isLocalPlayer()) {
            self->setPosition(Position());
        }

        self->m_oldPosition = Position();
        self->m_disappearEvent = nullptr;
    });
}

void Creature::onDeath()
{
    // Notify Lua scripts of the creature's death
    callLuaField("onDeath");
}

void Creature::updateWalkAnimation(int totalPixelsWalked, int stepDuration)
{
    // Update the walk animation based on the total pixels walked and step duration
    if (m_outfit.getCategory() != ThingCategoryCreature) return;

    int animationPhases = getAnimationPhases() - 1;
    int frameDelay = stepDuration / 3;

    // Adjust animation phases if the creature is mounted
    if (m_outfit.getMount() != 0) {
        ThingType* type = g_things.rawGetThingType(m_outfit.getMount(), m_outfit.getCategory());
        animationPhases = type->getAnimationPhases() - 1;
    }

    // Determine the current animation phase based on walking progress
    if (animationPhases == 0) {
        m_walkAnimationPhase = 0;
    } else if (m_footStepDrawn && m_footTimer.ticksElapsed() >= frameDelay && totalPixelsWalked < 32) {
        m_footStep++;
        m_walkAnimationPhase = 1 + (m_footStep % animationPhases);
        m_footStepDrawn = false;
        m_footTimer.restart();
    } else if (m_walkAnimationPhase == 0 && totalPixelsWalked < 32) {
        m_walkAnimationPhase = 1 + (m_footStep % animationPhases);
    }

    // Schedule an event to reset the animation phase when the walk is complete
    if (totalPixelsWalked == 32 && !m_walkFinishAnimEvent) {
        auto self = static_self_cast<Creature>();
        m_walkFinishAnimEvent = g_dispatcher.scheduleEvent([self] {
            if (!self->m_walking || self->m_walkTimer.ticksElapsed() >= self->getStepDuration(true)) {
                self->m_walkAnimationPhase = 0;
            }
            self->m_walkFinishAnimEvent = nullptr;
        }, std::min<int>(frameDelay, 200));
    }
}

void Creature::updateWalkOffset(int totalPixelsWalked)
{
    // Update the walk offset based on the direction and pixels walked
    m_walkOffset = Point(0, 0);

    if (m_direction & (Otc::North | Otc::NorthEast | Otc::NorthWest))
        m_walkOffset.y = 32 - totalPixelsWalked;
    else if (m_direction & (Otc::South | Otc::SouthEast | Otc::SouthWest))
        m_walkOffset.y = totalPixelsWalked - 32;

    if (m_direction & (Otc::East | Otc::NorthEast | Otc::SouthEast))
        m_walkOffset.x = totalPixelsWalked - 32;
    else if (m_direction & (Otc::West | Otc::NorthWest | Otc::SouthWest))
        m_walkOffset.x = 32 - totalPixelsWalked;
}

void Creature::setDirection(Otc::Direction direction)
{
    // Set the creature's direction, ensuring it's valid
    assert(direction != Otc::InvalidDirection);
    m_direction = direction;
}

void Creature::setName(const std::string& name)
{
    // Set the creature's name and update the name cache
    m_name = name;
    m_nameCache.setText(name);
}

void Creature::setHealthPercent(uint8 healthPercent)
{
    // Update the creature's health percentage and determine the information color
    static const std::vector<std::pair<uint8, Color>> healthLevels = {
        {93, Color(0x00, 0xBC, 0x00)}, {61, Color(0x50, 0xA1, 0x50)},
        {31, Color(0xA1, 0xA1, 0x00)}, {9, Color(0xBF, 0x0A, 0x0A)},
        {4, Color(0x91, 0x0F, 0x0F)}, {0, Color(0x85, 0x0C, 0x0C)}
    };
    
    for (const auto& [threshold, color] : healthLevels) {
        if (healthPercent >= threshold) {
            m_informationColor = color;
            break;
        }
    }
    
    m_healthPercent = healthPercent;
    callLuaField("onHealthPercentChange", healthPercent);
    if (healthPercent == 0) onDeath();
}

void Creature::setOutfit(const Outfit& outfit)
{
    // Update the creature's outfit and notify Lua scripts of the change
    Outfit previousOutfit = m_outfit;
    
    if (outfit.getCategory() == ThingCategoryCreature &&
        outfit.getId() > 0 && g_things.isValidDatId(outfit.getId(), ThingCategoryCreature)) {
        m_outfit = outfit;
    } else if (g_things.isValidDatId(outfit.getAuxId(), outfit.getCategory())) {
        m_outfit.setAuxId(outfit.getAuxId());
        m_outfit.setCategory(outfit.getCategory());
    }
    
    m_walkAnimationPhase = 0;
    callLuaField("onOutfitChange", m_outfit, previousOutfit);
}

void Creature::updateWalkingTile()
{
    // Update the tile the creature is walking on based on its position and offset
    TilePtr newTile = nullptr;
    Rect creatureArea(Otc::TILE_PIXELS + (m_walkOffset.x - getDisplacementX()),
                      Otc::TILE_PIXELS + (m_walkOffset.y - getDisplacementY()),
                      Otc::TILE_PIXELS, Otc::TILE_PIXELS);
    
    for (int dx = -1; dx <= 1 && !newTile; ++dx) {
        for (int dy = -1; dy <= 1 && !newTile; ++dy) {
            Rect tileArea((dx + 1) * Otc::TILE_PIXELS, (dy + 1) * Otc::TILE_PIXELS, Otc::TILE_PIXELS, Otc::TILE_PIXELS);
            if (tileArea.contains(creatureArea.bottomRight())) {
                newTile = g_map.getOrCreateTile(m_position.translated(dx, dy, 0));
            }
        }
    }

    // Update the walking tile and notify the map if necessary
    if (newTile != m_walkingTile) {
        if (m_walkingTile) m_walkingTile->removeWalkingCreature(static_self_cast<Creature>());
        if (newTile) {
            newTile->addWalkingCreature(static_self_cast<Creature>());
            if (newTile->isEmpty()) g_map.notificateTileUpdate(newTile->getPosition());
        }
        m_walkingTile = newTile;
    }
}

void Creature::nextWalkUpdate()
{
    // Schedule the next walk update event
    if (m_walkUpdateEvent) m_walkUpdateEvent->cancel();
    updateWalk();
    
    if (m_walking) {
        auto self = static_self_cast<Creature>();
        m_walkUpdateEvent = g_dispatcher.scheduleEvent([self] {
            self->m_walkUpdateEvent = nullptr;
            self->nextWalkUpdate();
        }, getStepDuration(true) / Otc::TILE_PIXELS);
    }
}

void Creature::updateWalk()
{
    // Update the walk progress and animation based on elapsed time
    int stepTime = getStepDuration(true);
    int traveledPixels = stepTime ? std::min<int>((m_walkTimer.ticksElapsed() * Otc::TILE_PIXELS) / stepTime, Otc::TILE_PIXELS) : 0;
    m_walkedPixels = std::max(m_walkedPixels, traveledPixels);
    
    updateWalkAnimation(traveledPixels, stepTime);
    updateWalkOffset(m_walkedPixels);
    updateWalkingTile();
    
    // Terminate walking if the step duration is complete
    if (m_walking && m_walkTimer.ticksElapsed() >= getStepDuration()) terminateWalk();
}

void Creature::terminateWalk()
{
    // Cancel any scheduled walk update event
    if (m_walkUpdateEvent) {
        m_walkUpdateEvent->cancel();
        m_walkUpdateEvent = nullptr;
    }
    
    // If there is a pending direction change, apply it
    if (m_walkTurnDirection != Otc::InvalidDirection) {
        setDirection(m_walkTurnDirection);
        m_walkTurnDirection = Otc::InvalidDirection;
    }
    
    // Remove the creature from the current walking tile
    if (m_walkingTile) {
        m_walkingTile->removeWalkingCreature(static_self_cast<Creature>());
        m_walkingTile = nullptr;
    }
    
    // Reset walking state and related attributes
    m_walking = false;
    m_walkedPixels = 0;
    m_walkOffset = Point(0,0);
    m_walkAnimationPhase = 0;
}

void Creature::setSpeed(uint16 speed)
{
    // Store the previous speed and update to the new speed
    uint16 previousSpeed = m_speed;
    m_speed = speed;
    
    // If the creature is walking, schedule the next walk update
    if (m_walking) nextWalkUpdate();
    
    // Notify Lua scripts about the speed change
    callLuaField("onSpeedChange", m_speed, previousSpeed);
}

void Creature::setBaseSpeed(double baseSpeed)
{
    // If the base speed hasn't changed, do nothing
    if (m_baseSpeed == baseSpeed) return;
    
    // Store the previous base speed and update to the new base speed
    double previousBaseSpeed = m_baseSpeed;
    m_baseSpeed = baseSpeed;
    
    // Notify Lua scripts about the base speed change
    callLuaField("onBaseSpeedChange", baseSpeed, previousBaseSpeed);
}

void Creature::setSkull(uint8 skull)
{
    // If the skull hasn't changed, do nothing
    if (m_skull == skull) return;
    
    // Update the skull and notify Lua scripts
    m_skull = skull;
    callLuaField("onSkullChange", skull);
}

void Creature::setShield(uint8 shield)
{
    // If the shield hasn't changed, do nothing
    if (m_shield == shield) return;
    
    // Update the shield and notify Lua scripts
    m_shield = shield;
    callLuaField("onShieldChange", shield);
}

void Creature::setEmblem(uint8 emblem)
{
    // If the emblem hasn't changed, do nothing
    if (m_emblem == emblem) return;
    
    // Update the emblem and notify Lua scripts
    m_emblem = emblem;
    callLuaField("onEmblemChange", emblem);
}

void Creature::setType(uint8 type)
{
    // If the type hasn't changed, do nothing
    if (m_type == type) return;
    
    // Update the type and notify Lua scripts
    m_type = type;
    callLuaField("onTypeChange", type);
}

void Creature::setIcon(uint8 icon)
{
    // If the icon hasn't changed, do nothing
    if (m_icon == icon) return;
    
    // Update the icon and notify Lua scripts
    m_icon = icon;
    callLuaField("onIconChange", icon);
}

void Creature::setSkullTexture(const std::string& filename)
{
    // If the filename is not empty, load the texture
    if (!filename.empty()) {
        m_skullTexture = g_textures.getTexture(filename);
    }
}

void Creature::setShieldTexture(const std::string& filename, bool blink)
{
    // If the filename is not empty, load the texture and set it to be shown
    if (!filename.empty()) {
        m_shieldTexture = g_textures.getTexture(filename);
        m_showShieldTexture = true;
    }
    
    // If blinking is enabled and not already blinking, schedule shield update events
    if (blink && !m_shieldBlink) {
        auto self = static_self_cast<Creature>();
        g_dispatcher.scheduleEvent([self]() { self->updateShield(); }, SHIELD_BLINK_TICKS);
    }
    
    // Set the shield blink state
    m_shieldBlink = blink;
}

void Creature::setEmblemTexture(const std::string& filename)
{
    // If the filename is not empty, load the texture
    if (!filename.empty()) {
        m_emblemTexture = g_textures.getTexture(filename);
    }
}

void Creature::setTypeTexture(const std::string& filename)
{
    // If the filename is not empty, load the texture
    if (!filename.empty()) {
        m_typeTexture = g_textures.getTexture(filename);
    }
}

void Creature::setIconTexture(const std::string& filename)
{
    // If the filename is not empty, load the texture
    if (!filename.empty()) {
        m_iconTexture = g_textures.getTexture(filename);
    }
}

void Creature::setSpeedFormula(double speedA, double speedB, double speedC)
{
    // Set the speed formula coefficients
    m_speedFormula[Otc::SpeedFormulaA] = speedA;
    m_speedFormula[Otc::SpeedFormulaB] = speedB;
    m_speedFormula[Otc::SpeedFormulaC] = speedC;
}

bool Creature::hasSpeedFormula()
{
    // Check if all speed formula coefficients are set
    return std::all_of(std::begin(m_speedFormula), std::end(m_speedFormula), [](double value) { return value != -1; });
}

void Creature::addTimedSquare(uint8 color)
{
    // Enable the timed square and set its color
    m_showTimedSquare = true;
    m_timedSquareColor = Color::from8bit(color);
    
    // Schedule an event to remove the timed square after a duration
    auto self = static_self_cast<Creature>();
    g_dispatcher.scheduleEvent([self]() { self->removeTimedSquare(); }, VOLATILE_SQUARE_DURATION);
}

void Creature::updateShield()
{
    // Toggle the visibility of the shield texture if blinking
    if (m_shieldBlink) {
        m_showShieldTexture = !m_showShieldTexture;
        
        // If the shield is not none, schedule the next shield update
        if (m_shield != Otc::ShieldNone) {
            auto self = static_self_cast<Creature>();
            g_dispatcher.scheduleEvent([self]() { self->updateShield(); }, SHIELD_BLINK_TICKS);
        }
    } else {
        // Ensure the shield texture is shown if not blinking
        m_showShieldTexture = true;
    }
}

Point Creature::getDrawOffset()
{
    // Calculate the draw offset based on walking state and tile elevation
    Point drawOffset;
    if (m_walking) {
        if (m_walkingTile) {
            drawOffset -= Point(1,1) * m_walkingTile->getDrawElevation();
        }
        drawOffset += m_walkOffset;
    } else if (const TilePtr& tile = getTile(); tile) {
        drawOffset -= Point(1,1) * tile->getDrawElevation();
    }
    return drawOffset;
}

int Creature::getStepDuration(bool ignoreDiagonal, Otc::Direction dir) {
    // Return 0 if speed is less than 1
    if (m_speed < 1) return 0;

    // Adjust speed based on game features
    int adjustedSpeed = g_game.getFeature(Otc::GameNewSpeedLaw) ? m_speed * 2 : m_speed;
    Position targetPos = (dir == Otc::InvalidDirection) ? m_lastStepToPosition : m_position.translatedToDirection(dir);
    if (!targetPos.isValid()) targetPos = m_position;

    // Determine ground speed from the target tile
    int groundSpeed = 150;
    if (const TilePtr& tile = g_map.getTile(targetPos)) {
        if (tile->getGroundSpeed() > 0)
            groundSpeed = tile->getGroundSpeed();
    }

    // Calculate step interval based on ground speed and adjusted speed
    int stepInterval = (groundSpeed > 0 && adjustedSpeed > 0) ? (1000 * groundSpeed) : 1000;
    if (g_game.getFeature(Otc::GameNewSpeedLaw) && hasSpeedFormula()) {
        int computedSpeed = std::max(1, (int)std::round(m_speedFormula[Otc::SpeedFormulaA] * log((adjustedSpeed / 2) + m_speedFormula[Otc::SpeedFormulaB]) + m_speedFormula[Otc::SpeedFormulaC]));
        stepInterval /= computedSpeed;
    } else {
        stepInterval /= adjustedSpeed;
    }

    // Adjust step interval for client version and server beat
    if (g_game.getClientVersion() >= 900)
        stepInterval = (stepInterval / g_game.getServerBeat()) * g_game.getServerBeat();

    // Apply diagonal factor if necessary
    float diagonalFactor = (g_game.getClientVersion() <= 810) ? 2.0f : 3.0f;
    stepInterval = std::max(stepInterval, g_game.getServerBeat());

    // If the movement is diagonal, apply a diagonal factor to the step interval
    if (!ignoreDiagonal && (m_lastStepDirection == Otc::NorthWest || m_lastStepDirection == Otc::NorthEast ||
                            m_lastStepDirection == Otc::SouthWest || m_lastStepDirection == Otc::SouthEast)) {
        stepInterval *= diagonalFactor;
    }

    return stepInterval;
}

Point Creature::getDisplacement() {
    // Return displacement based on outfit category
    switch (m_outfit.getCategory()) {
        case ThingCategoryEffect: return Point(8, 8);
        case ThingCategoryItem: return Point(0, 0);
        default: return Thing::getDisplacement();
    }
}

int Creature::getDisplacementX() {
    // Return X displacement based on outfit category and mount status
    if (m_outfit.getCategory() == ThingCategoryEffect) return 8;
    if (m_outfit.getCategory() == ThingCategoryItem) return 0;
    return (m_outfit.getMount() != 0) ? g_things.rawGetThingType(m_outfit.getMount(), ThingCategoryCreature)->getDisplacementX() : Thing::getDisplacementX();
}

int Creature::getDisplacementY() {
    // Return Y displacement based on outfit category and mount status
    if (m_outfit.getCategory() == ThingCategoryEffect) return 8;
    if (m_outfit.getCategory() == ThingCategoryItem) return 0;
    return (m_outfit.getMount() != 0) ? g_things.rawGetThingType(m_outfit.getMount(), ThingCategoryCreature)->getDisplacementY() : Thing::getDisplacementY();
}

int Creature::getExactSize(int layer, int xPattern, int yPattern, int zPattern, int animationPhase) {
    // Calculate the exact size of the creature's outfit
    int maxSize = 0;
    xPattern = Otc::South;
    zPattern = (m_outfit.getMount() != 0) ? 1 : 0;
    animationPhase = 0;

    // Iterate over each pattern and layer to determine the maximum size
    for (yPattern = 0; yPattern < getNumPatternY(); ++yPattern) {
        if (yPattern > 0 && !(m_outfit.getAddons() & (1 << (yPattern - 1)))) continue;
        for (layer = 0; layer < getLayers(); ++layer)
            maxSize = std::max(maxSize, Thing::getExactSize(layer, xPattern, yPattern, zPattern, animationPhase));
    }
    return maxSize;
}

const ThingTypePtr& Creature::getThingType() {
    // Get the ThingType for the creature's outfit
    return g_things.getThingType(m_outfit.getId(), ThingCategoryCreature);
}

ThingType* Creature::rawGetThingType() {
    // Get the raw ThingType for the creature's outfit
    return g_things.rawGetThingType(m_outfit.getId(), ThingCategoryCreature);
}