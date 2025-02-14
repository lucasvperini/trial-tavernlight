-- Q2 - Fix or improve the implementation of the below method
function printSmallGuildNames(memberCount)
    -- this method is supposed to print names of all guilds that have less than memberCount max members
    local selectGuildQuery = "SELECT name FROM guilds WHERE max_members < %d;"
    local resultId = db.storeQuery(string.format(selectGuildQuery, memberCount))
    local guildName = result.getString("name")
    print(guildName)
end

--------------------------------------------------------------------------------
-- Answer:

-- This method is supposed to print names of all guilds that have less than memberCount members;
-- There is no record in the standard schema in TFS SQL that stores a value for the historical maximum number of members or similar, I believe that with "max_members" you would like to know the total number of members currently.
-- If the objective of this function is to search for a historical value of max_members, we would first need to create a column to store this value, and then we could perform the query.
-- Another way to create this would be to initialize the column:
-- ALTER TABLE `guilds` ADD `max_members` INT NOT NULL DEFAULT 0;
-- And then, create a trigger to update the column every time a new member is added to the guild:
-- DELIMITER //
-- CREATE TRIGGER `update_max_members` AFTER INSERT ON `guild_membership`
-- FOR EACH ROW BEGIN
--     DECLARE current_member_count INT;
--     SELECT COUNT(*) INTO current_member_count FROM `guild_membership` WHERE `guild_id` = NEW.`guild_id`;
--     UPDATE `guilds` SET `max_members` = current_member_count
--     WHERE `id` = NEW.`guild_id` AND current_member_count > `max_members`;
-- END;
-- //
-- DELIMITER ;
-- This way, this historical record would automatically be updated.
-- The only change between the code for counting members and the version with the max_members column is a query that must be performed, the other changes apply to both.
-- The query for the historical member count would be:
-- local selectGuildQuery = string.format("SELECT name FROM guilds WHERE max_members < %d;", memberCount)

function printSmallGuildNames(memberCount)
	-- Check if memberCount is a number, exists, and is greater than 1. 
	-- Based on the function, any value below 1 as an argument is meaningless.
	if not memberCount or type(memberCount) ~= "number" or memberCount < 1 then
        print("Invalid member count.")
        return
    end

    -- Query based on actual member count
    local selectGuildQuery = string.format("SELECT g.name " ..
                                           "FROM guilds g " ..
                                           "JOIN guild_membership m ON g.id = m.guild_id " ..
                                           "GROUP BY g.id, g.name " ..
                                           "HAVING COUNT(m.player_id) < %d;", memberCount)

    -- Get the resultID from the query and check if it has results.
    local resultID = db.storeQuery(selectGuildQuery)
    if resultID == false then
        print("No guilds found with less than " .. memberCount .. " member(s).")
        return
    end

    -- Loop through the results, print each.
    repeat
        local guildName = result.getDataString(resultID, "name")
        print(guildName)
    until not result.next(resultID)

    result.free(resultID)
end