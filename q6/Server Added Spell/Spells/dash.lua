-- Define default spell attributes
local spellConfig = {
    speed = 5,
    maxRange = 5
}

-- Defines movement offsets based on direction
local movementOffset = {
    [DIRECTION_NORTH] = Position(0, -1),
    [DIRECTION_SOUTH] = Position(0, 1),
    [DIRECTION_WEST] = Position(-1, 0),
    [DIRECTION_EAST] = Position(1, 0),
    [DIRECTION_NORTHWEST] = Position(0, 0),
    [DIRECTION_NORTHEAST] = Position(0, 0),
    [DIRECTION_SOUTHWEST] = Position(0, 0),
    [DIRECTION_SOUTHEAST] = Position(0, 0)
}

-- Creates the spell instance
local spell = Spell(SPELL_INSTANT)
spell:id(777)
spell:name("Dash")
spell:words("dash")
spell:group("attack")
spell:isSelfTarget(true)
spell:needLearn(false)
spell:level(1)
spell:mana(0)
spell:cooldown(0)
spell:groupCooldown(0)

-- Handles the end of the dash effect
local function finalizeDash(creatureId)
    addEvent(function()
        local msg = NetworkMessage()
        msg:addByte(50) -- Extended OPcode
        msg:addByte(999) -- Dash state change
        msg:addString(tostring(creatureId) .. ",ed") -- "ed" signals dash end
        msg:sendToPlayer(Creature(creatureId))
        msg:delete()
    end, 5)
end

-- Executes the dash movement recursively
local function dash(creatureId, remainingSteps)
    local creature = Creature(creatureId)
    if not creature then return end

    local direction = creature:getDirection()
    local newPosition = creature:getPosition() + movementOffset[direction]
    local targetTile = Tile(newPosition)

    -- Check if the movement is possible
    if targetTile:hasFlag(bit.bor(TILESTATE_BLOCKPATH, TILESTATE_BLOCKSOLID, TILESTATE_PROTECTIONZONE, TILESTATE_TELEPORT, TILESTATE_MAGICFIELD, TILESTATE_FLOORCHANGE)) 
        or targetTile:getCreatureCount() > 0 then
        finalizeDash(creatureId)
        return
    end

    -- Notify client on first movement
    if remainingSteps == spellConfig.maxRange then
        local msg = NetworkMessage()
        msg:addByte(50) -- Extended OPcode
        msg:addByte(100) -- Dash state change
        msg:addString(tostring(creatureId) .. ",sd") -- "sd" signals dash start
        msg:sendToPlayer(creature)
        msg:delete()
    end

    -- Move creature and adjust its direction
    creature:teleportTo(newPosition, false)
    creature:setDirection(direction)
    
    -- Continue dashing if steps remain
    if remainingSteps > 1 then
        addEvent(dash, spellConfig.speed, creatureId, remainingSteps - 1)
    else
        finalizeDash(creatureId)
    end
end

-- Spell cast logic
function spell.onCastSpell(creature, variant)
    if spellConfig.maxRange > 0 then
        dash(creature:getId(), spellConfig.maxRange)
    end
    return false -- Prevents spell words from being displayed in chat
end

spell:register()