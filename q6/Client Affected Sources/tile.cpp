#include "tile.h"
#include "item.h"
#include "thingtypemanager.h"
#include "map.h"
#include "game.h"
#include "localplayer.h"
#include "effect.h"
#include "protocolgame.h"
#include "lightview.h"
#include <framework/graphics/fontmanager.h>

// Constructor for the Tile class, initializes position and other attributes
Tile::Tile(const Position& position)
    : m_position(position), m_drawElevation(0), m_minimapColor(0), m_flags(0) {}

// Cleans the tile by removing all things from it
void Tile::clean()
{
    while (!m_things.empty()) {
        removeThing(m_things.front());
    }
}

// Adds a walking creature to the tile
void Tile::addWalkingCreature(const CreaturePtr& creature)
{
    m_walkingCreatures.push_back(creature);
}

// Removes a walking creature from the tile
void Tile::removeWalkingCreature(const CreaturePtr& creature)
{
    m_walkingCreatures.erase(std::remove(m_walkingCreatures.begin(), m_walkingCreatures.end(), creature), m_walkingCreatures.end());
}

// Adds a thing to the tile at a specific stack position
void Tile::addThing(const ThingPtr& thing, int stackPos)
{
    if (!thing) return;

    // Handle effects separately from other things
    if (thing->isEffect()) {
        auto effect = thing->static_self_cast<Effect>();
        (thing->isTopEffect()) ? m_effects.insert(m_effects.begin(), effect) : m_effects.push_back(effect);
    } else {
        // Determine the priority and position for the new thing
        int priority = thing->getStackPriority();
        bool append = (stackPos < 0 || stackPos == 255) ? (priority <= 3) : false;
        
        if (stackPos < 0 || stackPos == 255) {
            if (g_game.getClientVersion() >= 854 && priority == 4) {
                append = !append;
            }
            
            for (stackPos = 0; stackPos < static_cast<int>(m_things.size()); ++stackPos) {
                int otherPriority = m_things[stackPos]->getStackPriority();
                if ((append && otherPriority > priority) || (!append && otherPriority >= priority)) {
                    break;
                }
            }
        }
        
        stackPos = std::min(stackPos, static_cast<int>(m_things.size()));
        m_things.insert(m_things.begin() + stackPos, thing);
        
        // Ensure the number of things does not exceed the maximum allowed
        if (m_things.size() > MAX_THINGS) {
            removeThing(m_things[MAX_THINGS]);
        }
    }
    
    // Set the position of the thing and trigger its appearance
    thing->setPosition(m_position);
    thing->onAppear();
    
    // Check if the thing is translucent and update light if necessary
    if (thing->isTranslucent()) {
        checkTranslucentLight();
    }
}

// Draws the tile and its contents on the screen
void Tile::draw(const Point& dest, float scaleFactor, int drawFlags, std::vector<LocalEffect> localEffects, LightView *lightView)
{
    bool animate = drawFlags & Otc::DrawAnimations;
    m_drawElevation = 0;
    
    // Define zone flags for special tile states
    static const tileflags_t zoneFlags[] = {
        TILESTATE_HOUSE, TILESTATE_PROTECTIONZONE, TILESTATE_OPTIONALZONE, TILESTATE_HARDCOREZONE,
        TILESTATE_REFRESH, TILESTATE_NOLOGOUT, TILESTATE_LAST
    };

    // Lambda function to draw things within a specified range
    auto drawThings = [&](const auto& range) {
        for (const ThingPtr& thing : range) {
            if (!thing->isGround() && !thing->isGroundBorder() && !thing->isOnBottom()) break;

            bool restoreColor = false;
            if (g_map.showZones() && thing->isGround()) {
                for (auto flag : zoneFlags) {
                    if (hasFlag(flag) && g_map.showZone(flag)) {
                        g_painter->setOpacity(g_map.getZoneOpacity());
                        g_painter->setColor(g_map.getZoneColor(flag));
                        restoreColor = true;
                        break;
                    }
                }
            }

            if (m_selected) g_painter->setColor(Color::teal);
            thing->draw(dest - m_drawElevation * scaleFactor, scaleFactor, animate, lightView);
            if (restoreColor) g_painter->resetOpacity(), g_painter->resetColor();
            if (m_selected) g_painter->resetColor();

            m_drawElevation = std::min(m_drawElevation + thing->getElevation(), Otc::MAX_ELEVATION);
        }
    };

    // Draw different layers of the tile based on flags
    if (drawFlags & (Otc::DrawGround | Otc::DrawGroundBorders | Otc::DrawOnBottom)) {
        drawThings(m_things);
    }

    if (drawFlags & Otc::DrawItems) {
        drawThings(std::vector<ThingPtr>(m_things.rbegin(), m_things.rend()));
    }

    // Draw local effects such as afterimages
    for (const LocalEffect& effect : localEffects) {
        if (effect.m_type == LocalEffect::LocalEffectType_Afterimage) {
            auto creature = effect.m_thing->static_self_cast<Creature>();
            Point pos = dest + effect.m_data.afterimage.m_offset +
                        Point((effect.m_data.afterimage.m_position.x - m_position.x) * Otc::TILE_PIXELS - m_drawElevation,
                              (effect.m_data.afterimage.m_position.y - m_position.y) * Otc::TILE_PIXELS - m_drawElevation) * scaleFactor;
            creature->drawAfterimage(pos, scaleFactor, effect.m_data.afterimage);
        }
    }

    // Draw creatures on the tile
    if (drawFlags & Otc::DrawCreatures) {
        for (const auto& creature : m_walkingCreatures) {
            Point pos = dest + Point((creature->getPosition().x - m_position.x) * Otc::TILE_PIXELS - m_drawElevation,
                                     (creature->getPosition().y - m_position.y) * Otc::TILE_PIXELS - m_drawElevation) * scaleFactor;
            creature->draw(pos, scaleFactor, animate, lightView);
        }

        for (auto it = m_things.rbegin(); it != m_things.rend(); ++it) {
            if (!(*it)->isCreature()) continue;
            auto creature = (*it)->static_self_cast<Creature>();
            if (creature && (!creature->isWalking() || !animate)) {
                creature->draw(dest - m_drawElevation * scaleFactor, scaleFactor, animate, lightView);
            }
        }
    }

    // Draw effects on the tile
    if (drawFlags & Otc::DrawEffects) {
        for (const auto& effect : m_effects) {
            effect->drawEffect(dest - m_drawElevation * scaleFactor, scaleFactor, animate,
                               m_position.x - g_map.getCentralPosition().x,
                               m_position.y - g_map.getCentralPosition().y, lightView);
        }
    }

    // Draw things that are on top of others
    if (drawFlags & Otc::DrawOnTop) {
        for (const auto& thing : m_things) {
            if (thing->isOnTop()) thing->draw(dest, scaleFactor, animate, lightView);
        }
    }

    // Add light source if the tile has translucent light
    if (hasTranslucentLight() && lightView) {
        lightView->addLightSource(dest + Point(16, 16) * scaleFactor, scaleFactor, {1});
    }
}

// Removes a thing from the tile
bool Tile::removeThing(ThingPtr thing)
{
    if (!thing)
        return false;

    // Helper lambda to remove an item from a list
    auto removeFromList = [](auto& list, auto item) -> bool {
        auto it = std::find(list.begin(), list.end(), item);
        if (it != list.end()) {
            list.erase(it);
            return true;
        }
        return false;
    };

    // Remove the thing from the appropriate list
    bool removed = thing->isEffect() ? removeFromList(m_effects, thing->static_self_cast<Effect>())
                                     : removeFromList(m_things, thing);

    if (removed) {
        thing->onDisappear();
        if (thing->isTranslucent())
            checkTranslucentLight();
    }
    
    return removed;
}

// Retrieves a thing from the tile at a specific stack position
ThingPtr Tile::getThing(int stackPos)
{
    return (stackPos >= 0 && stackPos < static_cast<int>(m_things.size())) ? m_things[stackPos] : nullptr;
}

// Retrieves an effect from the tile by its ID
EffectPtr Tile::getEffect(uint16 id)
{
    auto it = std::find_if(m_effects.begin(), m_effects.end(), [id](const EffectPtr& effect) {
        return effect->getId() == id;
    });
    return (it != m_effects.end()) ? *it : nullptr;
}

// Checks if the tile contains a specific thing
bool Tile::hasThing(const ThingPtr& thing)
{
    return std::any_of(m_things.begin(), m_things.end(), [&thing](const ThingPtr& t) { return t == thing; });
}

// Gets the stack position of a specific thing on the tile
int Tile::getThingStackPos(const ThingPtr& thing)
{
    auto it = std::find(m_things.begin(), m_things.end(), thing);
    return (it != m_things.end()) ? std::distance(m_things.begin(), it) : -1;
}

// Retrieves the topmost thing on the tile
ThingPtr Tile::getTopThing()
{
    if (isEmpty())
        return nullptr;
    
    auto it = std::find_if(m_things.begin(), m_things.end(), [](const ThingPtr& thing) {
        return !(thing->isGround() || thing->isGroundBorder() || thing->isOnBottom() || thing->isOnTop() || thing->isCreature());
    });
    return (it != m_things.end()) ? *it : m_things.back();
}

// Retrieves all items on the tile
std::vector<ItemPtr> Tile::getItems()
{
    std::vector<ItemPtr> items;
    std::copy_if(m_things.begin(), m_things.end(), std::back_inserter(items), [](const ThingPtr& thing) {
        return thing->isItem();
    });
    return items;
}

// Retrieves all creatures on the tile
std::vector<CreaturePtr> Tile::getCreatures()
{
    std::vector<CreaturePtr> creatures;
    for (const auto& thing : m_things) {
        if (thing->isCreature()) {
            creatures.push_back(thing->static_self_cast<Creature>());
        }
    }
    return creatures;
}

// Retrieves the ground item on the tile
ItemPtr Tile::getGround()
{
    return (m_things.empty() || !m_things[0]->isGround() || !m_things[0]->isItem()) ? nullptr : m_things[0]->static_self_cast<Item>();
}

// Gets the speed of the ground item on the tile
int Tile::getGroundSpeed()
{
    ItemPtr ground = getGround();
    return ground ? ground->getGroundSpeed() : 100;
}

// Gets the minimap color byte for the tile
uint8 Tile::getMinimapColorByte()
{
    if (m_minimapColor != 0)
        return m_minimapColor;

    for (const auto& thing : m_things) {
        if (!(thing->isGround() || thing->isGroundBorder() || thing->isOnBottom() || thing->isOnTop()))
            break;
        if (uint8 c = thing->getMinimapColor(); c != 0)
            return c;
    }
    return 255;
}

// Retrieves the topmost thing that can be looked at on the tile
ThingPtr Tile::getTopLookThing()
{
    return isEmpty() ? nullptr : *std::find_if(m_things.begin(), m_things.end(), [](const ThingPtr& thing) {
        return !thing->isIgnoreLook() && !(thing->isGround() || thing->isGroundBorder() || thing->isOnBottom() || thing->isOnTop());
    });
}

// Retrieves the topmost thing that can be used on the tile
ThingPtr Tile::getTopUseThing()
{
    if (isEmpty())
        return nullptr;

    for (const auto& thing : m_things) {
        if (thing->isForceUse() || !(thing->isGround() || thing->isGroundBorder() || thing->isOnBottom() || thing->isOnTop() || thing->isCreature() || thing->isSplash()))
            return thing;
    }
    for (const auto& thing : m_things) {
        if (!(thing->isGround() || thing->isGroundBorder() || thing->isCreature() || thing->isSplash()))
            return thing;
    }
    return m_things[0];
}

// Retrieves the topmost creature on the tile
CreaturePtr Tile::getTopCreature()
{
    CreaturePtr creature;
    for (const auto& thing : m_things) {
        if (thing->isLocalPlayer())
            creature = thing->static_self_cast<Creature>();
        else if (thing->isCreature())
            return thing->static_self_cast<Creature>();
    }
    if (!creature && !m_walkingCreatures.empty())
        creature = m_walkingCreatures.back();

    if (!creature) {
        for (int xi = -1; xi <= 1; ++xi) {
            for (int yi = -1; yi <= 1; ++yi) {
                Position pos = m_position.translated(xi, yi);
                if (pos == m_position)
                    continue;

                if (TilePtr tile = g_map.getTile(pos)) {
                    for (const auto& c : tile->getCreatures()) {
                        if (c->isWalking() && c->getLastStepFromPosition() == m_position && c->getStepProgress() < 0.75f) {
                            creature = c;
                        }
                    }
                }
            }
        }
    }
    return creature;
}

// Retrieves the topmost thing that can be moved on the tile
ThingPtr Tile::getTopMoveThing()
{
    if (isEmpty())
        return nullptr;

    for (size_t i = 0; i < m_things.size(); ++i) {
        if (!(m_things[i]->isGround() || m_things[i]->isGroundBorder() || m_things[i]->isOnBottom() || m_things[i]->isOnTop() || m_things[i]->isCreature())) {
            return (i > 0 && m_things[i]->isNotMoveable()) ? m_things[i - 1] : m_things[i];
        }
    }

    auto it = std::find_if(m_things.begin(), m_things.end(), [](const ThingPtr& thing) {
        return thing->isCreature();
    });
    return (it != m_things.end()) ? *it : m_things[0];
}

// Retrieves the topmost thing that can be used for multiple purposes on the tile
ThingPtr Tile::getTopMultiUseThing()
{
    if (isEmpty()) return nullptr;

    if (auto topCreature = getTopCreature()) return topCreature;

    for (const auto& thing : m_things)
        if (thing->isForceUse()) return thing;

    for (size_t i = 0; i < m_things.size(); ++i) {
        auto thing = m_things[i];
        if (!(thing->isGround() || thing->isGroundBorder() || thing->isOnBottom() || thing->isOnTop())) {
            return (i > 0 && thing->isSplash()) ? m_things[i - 1] : thing;
        }
    }

    for (const auto& thing : m_things)
        if (!(thing->isGround() || thing->isOnTop())) return thing;

    return m_things.front();
}

// Checks if the tile is walkable, optionally ignoring creatures
bool Tile::isWalkable(bool ignoreCreatures)
{
    if (!getGround()) return false;

    for (const auto& thing : m_things) {
        if (thing->isNotWalkable()) return false;
        if (!ignoreCreatures && thing->isCreature()) {
            auto creature = thing->static_self_cast<Creature>();
            if (!creature->isPassable() && creature->canBeSeen()) return false;
        }
    }
    return true;
}

// Checks if the tile is pathable
bool Tile::isPathable()
{
    return std::none_of(m_things.begin(), m_things.end(), [](const ThingPtr& thing) { return thing->isNotPathable(); });
}

// Checks if the tile has full ground coverage
bool Tile::isFullGround()
{
    return getGround() && getGround()->isFullGround();
}

// Checks if the tile is fully opaque
bool Tile::isFullyOpaque()
{
    auto firstObject = getThing(0);
    return firstObject && firstObject->isFullGround();
}

// Checks if the tile is single dimension (1x1)
bool Tile::isSingleDimension()
{
    if (!m_walkingCreatures.empty()) return false;
    return std::all_of(m_things.begin(), m_things.end(), [](const ThingPtr& thing) { return thing->getHeight() == 1 && thing->getWidth() == 1; });
}

// Checks if the tile can be looked at
bool Tile::isLookPossible()
{
    return std::none_of(m_things.begin(), m_things.end(), [](const ThingPtr& thing) { return thing->blockProjectile(); });
}

// Checks if the tile is clickable
bool Tile::isClickable()
{
    bool hasGround = false, hasOnBottom = false;
    for (const auto& thing : m_things) {
        if (thing->isGround()) hasGround = true;
        if (thing->isOnBottom()) hasOnBottom = true;
        if ((hasGround || hasOnBottom)) return true;
    }
    return false;
}

// Checks if the tile is empty
bool Tile::isEmpty()
{
    return m_things.empty();
}

// Checks if the tile is drawable
bool Tile::isDrawable()
{
    return !isEmpty() || !m_walkingCreatures.empty() || !m_effects.empty();
}

// Checks if the tile must hook east
bool Tile::mustHookEast()
{
    return std::any_of(m_things.begin(), m_things.end(), [](const ThingPtr& thing) { return thing->isHookEast(); });
}

// Checks if the tile must hook south
bool Tile::mustHookSouth()
{
    return std::any_of(m_things.begin(), m_things.end(), [](const ThingPtr& thing) { return thing->isHookSouth(); });
}

// Checks if the tile has a creature
bool Tile::hasCreature()
{
    return std::any_of(m_things.begin(), m_things.end(), [](const ThingPtr& thing) { return thing->isCreature(); });
}

// Checks if the tile limits the view of floors
bool Tile::limitsFloorsView(bool isFreeView)
{
    auto firstThing = getThing(0);
    if (!firstThing || firstThing->isDontHide()) return false;

    return firstThing->isGround() || (firstThing->isOnBottom() && (!isFreeView || firstThing->blockProjectile()));
}

// Checks if the tile can be erased
bool Tile::canErase()
{
    return isEmpty() && m_walkingCreatures.empty() && m_effects.empty() && m_flags == 0 && m_minimapColor == 0;
}

// Gets the elevation of the tile
int Tile::getElevation() const
{
    return std::count_if(m_things.begin(), m_things.end(), [](const ThingPtr& thing) { return thing->getElevation() > 0; });
}

// Checks if the tile has a certain elevation
bool Tile::hasElevation(int elevation)
{
    return getElevation() >= elevation;
}

// Checks and updates the translucent light state of the tile
void Tile::checkTranslucentLight()
{
    if (m_position.z != Otc::SEA_FLOOR) return;

    Position downPos = m_position;
    if (!downPos.down()) return;

    auto tile = g_map.getOrCreateTile(downPos);
    if (!tile) return;

    bool hasTranslucent = std::any_of(m_things.begin(), m_things.end(), [](const ThingPtr& thing) {
        return thing->isTranslucent() || thing->hasLensHelp();
    });

    if (hasTranslucent)
        tile->m_flags |= TILESTATE_TRANSLUECENT_LIGHT;
    else
        tile->m_flags &= ~TILESTATE_TRANSLUECENT_LIGHT;
}