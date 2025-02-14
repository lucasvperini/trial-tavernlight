-- Default configuration for the custom spell.
local config = {
  damageType  = COMBAT_ICEDAMAGE, -- Type of damage the spell will deal.
  visualEffect = CONST_ME_ICETORNADO, -- Visual effect for the spell.
  spawnChance = 60, -- Probability (%) of creating a damage instance.
  totalWaves = 6, -- Number of waves the spell will cast.
  waveInterval = 300 -- Delay (ms) between each wave after the first one.
}

-- Custom formula for calculating damage dealt by the spell.
function onGetFormulaValues(caster, level, magicLevel)
  local min = (level / 10) + (magicLevel * 2.5) + 50
	local max = (level / 10) + (magicLevel * 5) + 100
	return -min, -max -- Min and max damage values (Random generic values, just for demonstration).
end

-- Initialize the tornado object for the spell, used to apply the damage.
local tornadoObject = Combat()
tornadoObject:setParameter(COMBAT_PARAM_TYPE, config.damageType)
tornadoObject:setParameter(COMBAT_PARAM_EFFECT, config.visualEffect)
tornadoObject:setParameter(COMBAT_PARAM_AGGRESSIVE, true)
tornadoObject:setCallback(CALLBACK_PARAM_LEVELMAGICVALUE, "onGetFormulaValues")

-- Create the small tornado object to define the area where tornado instances can spawn.
local smallTornadoObject = Combat()
smallTornadoObject:setParameter(COMBAT_PARAM_AGGRESSIVE, false)

-- Define the area of effect. The "2" marks the caster's position and is ignored for targeting.
local areaMatrix = {
  {0, 0, 0, 0, 1, 0, 0, 0, 0},
  {0, 0, 0, 1, 1, 1, 0, 0, 0},
  {0, 0, 1, 1, 1, 1, 1, 0, 0},
  {0, 1, 1, 1, 1, 1, 1, 1, 0},
  {1, 1, 1, 1, 2, 1, 1, 1, 1},
  {0, 1, 1, 1, 1, 1, 1, 1, 0},
  {0, 0, 1, 1, 1, 1, 1, 0, 0},
  {0, 0, 0, 1, 1, 1, 0, 0, 0},
  {0, 0, 0, 0, 1, 0, 0, 0, 0}
}

smallTornadoObject:setArea(createCombatArea(areaMatrix))

-- Function called on each valid tile within the wave's area.
function onTargetTile(caster, position)
  if math.random(0, 100) < config.spawnChance then
      tornadoObject:execute(caster, Variant(position)) -- Cast the tornado spell at the position.
  end
end

smallTornadoObject:setCallback(CALLBACK_PARAM_TARGETTILE, "onTargetTile")

-- Function to execute the spell logic, handling wave casting and timing.
function executeWave(casterId, variant)
  local caster = Creature(casterId)

  -- Ensure the caster is still valid.
  if not caster then
      return
  end

  -- Execute the wave, which triggers "onTargetTile" on each tile.
  smallTornadoObject:execute(caster, variant)
end

function onCastSpell(caster, variant)
  -- Cast the first wave if "totalWaves" is greater than 0.
  if config.totalWaves > 0 then
      executeWave(caster:getId(), variant)
  end

  -- Schedule additional waves after the initial one.
  for i = 1, config.totalWaves - 1 do
      addEvent(executeWave, i * config.waveInterval, caster:getId(), variant)
  end

  return true
end