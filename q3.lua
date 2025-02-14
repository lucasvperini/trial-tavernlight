-- Q3 - Fix or improve the name and the implementation of the below method
function do_sth_with_PlayerParty(playerId, membername)
    player = Player(playerId)
    local party = player:getParty()

    for k, v in pairs(party:getMembers()) do
        if v == Player(membername) then
            party:removeMember(Player(membername))
        end
    end
end

--------------------------------------------------------------------------------
-- Answer:
-- The function is designed to remove a member from a player's party based on the member's name. 
-- Main changes:
-- Renaming the function to one that allows you to understand what it does just by reading its name.
-- Check if the membername and the playerID are valid, and the party exists before making any changes.
-- Early exit from the player search loop in the party, to avoid unnecessary resource allocation.
-- More efficient comparison of players by avoiding unnecessary creation of a player object at each iteration.

function removePlayerFromPartyByName(playerID, memberName)
    if not memberName or memberName == "" then
        -- Member name is invalid or empty.
        return false
    end

    local player = Player(playerID)
    if not player then
        -- Player with ID doesn't exist
        return false
    end

    local playerParty = player:getParty()
    if not playerParty then
        -- Player is not in a party
        return false
    end

    for _, member in pairs(playerParty:getMembers()) do
        if member:getName() == memberName then
            -- We have found the player, remove it from the party and return true.
            playerParty:removeMember(member)
            return true
        end
    end

    return false
end
