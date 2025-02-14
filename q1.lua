-- Q1 - Fix or improve the implementation of the below methods
local function releaseStorage(player)
  player:setStorageValue(1000, -1)
end

function onLogout(player)
  if player:getStorageValue(1000) == 1 then
      addEvent(releaseStorage, 1000, player)
  end
  return true
end

--------------------------------------------------------------------------------
-- Answer:
-- Add some variables, to prevent "magic numbers".
local storageID = 1000
local releaseDelay = 5000

local function releaseStorage(playerGuid)
    -- Directly executes the change in the database asynchronously to avoid blocking any thread if the player is offline. 
    -- Additionally, since it's not possible to manipulate player storage while they are offline, we need to clear the data manually.
    -- The server will automatically clear the row with the '-1' value when the player logout again.

		-- If player variable return something, the player is online, else, the player is offline so we execute the query.
    local player = Player(playerGuid)
    if player then
        player:setStorageValue(storageID, -1)
    else
        db.asyncQuery("UPDATE player_storage SET value = -1 WHERE player_id = " .. db.escapeString(playerGuid) .. " AND `key` = " .. db.escapeString(storageID))
    end
end

function onLogout(player)
    -- Check if the player has a value of '1' in the storage.
    -- if player:getStorageValue(storageID) == 1 then
    addEvent(releaseStorage, releaseDelay, player:getGuid())
    -- end

    return true
end
